//! HTTP/1 parsing with stack-only or caller-owned output through a C ABI.
use std::{hint::black_box, mem::MaybeUninit, slice};

/// The caller supplies valid immutable bytes for the full call.
#[no_mangle]
pub unsafe extern "C" fn actix_web_request(
    _context: *mut *mut std::ffi::c_void, data: *const u8, len: usize,
) -> i32 {
    let bytes = unsafe { slice::from_raw_parts(data, len) };
    let mut headers = [MaybeUninit::uninit(); 100];
    let mut request = httparse::Request::new(&mut []);
    let result = request.parse_with_uninit_headers(bytes, &mut headers);
    black_box(&request);
    i32::from(!matches!(result, Ok(httparse::Status::Complete(_))))
}

/// The caller supplies valid immutable bytes for the full call.
#[no_mangle]
pub unsafe extern "C" fn actix_web_response(
    _context: *mut *mut std::ffi::c_void, data: *const u8, len: usize,
) -> i32 {
    let bytes = unsafe { slice::from_raw_parts(data, len) };
    let mut headers = [MaybeUninit::uninit(); 100];
    let mut response = httparse::Response::new(&mut []);
    let result = httparse::ParserConfig::default()
        .parse_response_with_uninit_headers(&mut response, bytes, &mut headers);
    black_box(&response);
    i32::from(!matches!(result, Ok(httparse::Status::Complete(_))))
}

#[no_mangle]
pub extern "C" fn actix_web_context_free(_context: *mut std::ffi::c_void) {}
