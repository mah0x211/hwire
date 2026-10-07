//! Reuse Milo's native event buffer; time reset and header parsing.
use std::{cell::RefCell, ffi::c_void, hint::black_box};
use milo_parser::{Parser, ERROR_NONE, STATE_BODY_DECISION};

thread_local! {
    // Native construction allocates an event buffer even with events disabled.
    // Warmup creates it once; each operation resets the reusable parser.
    static PARSER: RefCell<Parser> = RefCell::new({
        let mut parser = Parser::new();
        parser.autodetect = false;
        parser.suspend_after_headers = true;
        parser
    });
}

fn parse(data: *const u8, len: usize, request: bool) -> i32 {
    PARSER.with_borrow_mut(|parser| {
        parser.reset(false);
        parser.is_request = request;
        parser.parse(data, len);
        black_box(&*parser);
        i32::from(parser.error_code != ERROR_NONE || parser.state != STATE_BODY_DECISION)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn milo_request(_context: *mut *mut c_void, data: *const u8, len: usize) -> i32 {
    parse(data, len, true)
}

#[unsafe(no_mangle)]
pub extern "C" fn milo_response(_context: *mut *mut c_void, data: *const u8, len: usize) -> i32 {
    parse(data, len, false)
}

#[unsafe(no_mangle)]
pub extern "C" fn milo_context_free(_context: *mut c_void) {}
