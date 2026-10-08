//! Actix Web request storage and string lookup through the registration ABI.
use std::{mem::MaybeUninit, slice};

// The caller keeps the receive-buffer bytes alive until context destruction.
// Bytes::from_owner supplies shared ownership metadata without copying payload.
use actix_http::{Request, Method, Uri, Version, header::{HeaderName, HeaderValue}};
use bytes::Bytes;

// Initialize the native pool outside timing without retaining an arena-backed
// head in it. This guard occupies its initial heap-backed head for the run.
thread_local! {
    static POOL_GUARD: Request = Request::new();
}

// Actix's decoder records spans before converting the receive buffer to Bytes.
// Its HeaderIndex is crate-private; retain the same layout and recording steps.
#[derive(Clone, Copy)]
struct HeaderIndex {
    name: (usize, usize),
    value: (usize, usize),
}

const MAX_HEADERS: usize = 96;
const EMPTY_HEADER_INDEX: HeaderIndex = HeaderIndex { name: (0, 0), value: (0, 0) };

fn record_headers(data: *const u8, headers: &[httparse::Header<'_>],
                  indices: &mut [HeaderIndex]) {
    for (header, index) in headers.iter().zip(indices.iter_mut()) {
        let name = header.name.as_ptr() as usize - data as usize;
        let value = header.value.as_ptr() as usize - data as usize;
        *index = HeaderIndex {
            name: (name, name + header.name.len()),
            value: (value, value + header.value.len()),
        };
    }
}

unsafe fn store_headers(data: *const u8, len: usize,
                        headers: &[HeaderIndex]) -> Request {
    let bytes: &'static [u8] = unsafe { slice::from_raw_parts(data, len) };
    let owner = Bytes::from_owner(bytes);
    let mut message = Request::new();
    for h in headers {
        let name = HeaderName::from_bytes(&owner[h.name.0..h.name.1]).unwrap();
        // httparse already validates header values, as in Actix's decoder.
        let value = unsafe {
            HeaderValue::from_maybe_shared_unchecked(owner.slice(h.value.0..h.value.1))
        };
        message.headers_mut().append(name, value);
    }
    message
}

#[no_mangle]
pub unsafe extern "C" fn actix_web_request_with_store(
    context: *mut *mut Request, data: *const u8, len: usize, _header_capacity: usize,
) -> i32 {
    let bytes = unsafe { slice::from_raw_parts(data, len) };
    let mut indices = [EMPTY_HEADER_INDEX; MAX_HEADERS];
    let mut headers = [MaybeUninit::uninit(); MAX_HEADERS];
    let mut request = httparse::Request::new(&mut []);
    match request.parse_with_uninit_headers(bytes, &mut headers) {
        Ok(httparse::Status::Complete(_)) => {
            let method = Method::from_bytes(request.method.unwrap().as_bytes()).unwrap();
            let uri = Uri::try_from(request.path.unwrap()).unwrap();
            let version = if request.version.unwrap() == 1 {
                Version::HTTP_11
            } else {
                Version::HTTP_10
            };
            record_headers(data, request.headers, &mut indices);
            let mut message = unsafe { store_headers(data, len, &indices[..request.headers.len()]) };
            let head = message.head_mut();
            head.method = method;
            head.uri = uri;
            head.version = version;
            unsafe { *context = Box::into_raw(Box::new(message)); }
            0
        }
        _ => -1,
    }
}

#[no_mangle]
pub unsafe extern "C" fn actix_web_request_with_store_split(
    context: *mut *mut Request, data: *const u8, len: usize,
    _header_capacity: usize, split_at: usize,
) -> i32 {
    let bytes = unsafe { slice::from_raw_parts(data, len) };
    let mut indices = [EMPTY_HEADER_INDEX; MAX_HEADERS];
    let mut headers = [MaybeUninit::uninit(); MAX_HEADERS];
    let mut request = httparse::Request::new(&mut []);
    if !matches!(request.parse_with_uninit_headers(&bytes[..split_at], &mut headers),
                 Ok(httparse::Status::Partial)) {
        return -1;
    }
    // The native decoder starts a fresh httparse request on each retry.
    request = httparse::Request::new(&mut []);
    match request.parse_with_uninit_headers(bytes, &mut headers) {
        Ok(httparse::Status::Complete(_)) => {
            let method = Method::from_bytes(request.method.unwrap().as_bytes()).unwrap();
            let uri = Uri::try_from(request.path.unwrap()).unwrap();
            let version = if request.version.unwrap() == 1 {
                Version::HTTP_11
            } else {
                Version::HTTP_10
            };
            record_headers(data, request.headers, &mut indices);
            let mut message = unsafe { store_headers(data, len, &indices[..request.headers.len()]) };
            let head = message.head_mut();
            head.method = method;
            head.uri = uri;
            head.version = version;
            unsafe { *context = Box::into_raw(Box::new(message)); }
            0
        }
        _ => -1,
    }
}

#[no_mangle]
pub unsafe extern "C" fn actix_web_context_free(context: *mut Request) {
    if !context.is_null() {
        let message = unsafe { *Box::from_raw(context) };
        let (mut head, payload) = message.into_parts();
        drop(payload);
        head.headers.clear();
        head.uri = Uri::default();
        head.method = Method::default();
        // The arena reclaims this head. Returning it to the native pool would
        // retain pointers into memory reset before the next operation.
        #[cfg(any(target_os = "linux", target_os = "macos"))]
        std::mem::forget(head);
        #[cfg(not(any(target_os = "linux", target_os = "macos")))]
        drop(head);
    }
}

// Native request-pool setup is process initialization, outside measurements.
#[no_mangle]
pub extern "C" fn actix_web_store_init() {
    POOL_GUARD.with(|_| ());
}

// The caller supplies valid ASCII header names for the duration of the call.
#[no_mangle]
pub unsafe extern "C" fn actix_web_header_lookup(context: *const Request,
                                               key: *const u8, len: usize) -> usize {
    let key = unsafe { std::str::from_utf8_unchecked(slice::from_raw_parts(key, len)) };
    // Use the application's string API; HeaderName conversion is timed.
    unsafe { (*context).head().headers.get(key).map_or(0, |value| value.len() + 1) }
}

// HeaderName is immutable and contains no request-specific data.
#[no_mangle]
pub unsafe extern "C" fn actix_web_header_query_new(key: *const u8, len: usize) -> *mut HeaderName {
    let bytes = unsafe { slice::from_raw_parts(key, len) };
    match HeaderName::from_bytes(bytes) {
        Ok(name) => Box::into_raw(Box::new(name)),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn actix_web_header_query_free(query: *mut HeaderName) {
    if !query.is_null() {
        drop(unsafe { Box::from_raw(query) });
    }
}

#[no_mangle]
pub unsafe extern "C" fn actix_web_header_lookup_prepared(
    context: *const Request, query: *const HeaderName,
) -> usize {
    unsafe { (*context).head().headers.get(&*query).map_or(0, |value| value.len() + 1) }
}
