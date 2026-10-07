//! Stack-only HTTP header parsing through the suite's C ABI.
const std = @import("std");
const hparse = @import("hparse");

export fn hparse_request(_: *?*anyopaque, data: [*]const u8, len: usize) c_int {
    var method: hparse.Method = .unknown;
    var path: ?[]const u8 = null;
    var version: hparse.Version = .@"1.0";
    var headers: [100]hparse.Header = undefined;
    var count: usize = 0;
    _ = hparse.parseRequest(data[0..len], &method, &path, &version, &headers, &count) catch return 1;
    std.mem.doNotOptimizeAway(method);
    std.mem.doNotOptimizeAway(path);
    std.mem.doNotOptimizeAway(version);
    std.mem.doNotOptimizeAway(headers[0..count]);
    return 0;
}

export fn hparse_response(_: *?*anyopaque, data: [*]const u8, len: usize) c_int {
    var version: hparse.Version = .@"1.0";
    var status: u16 = 0;
    var reason: ?[]const u8 = null;
    var headers: [100]hparse.Header = undefined;
    var count: usize = 0;
    _ = hparse.parseResponse(data[0..len], &version, &status, &reason, &headers, &count) catch return 1;
    std.mem.doNotOptimizeAway(version);
    std.mem.doNotOptimizeAway(status);
    std.mem.doNotOptimizeAway(reason);
    std.mem.doNotOptimizeAway(headers[0..count]);
    return 0;
}

export fn hparse_context_free(_: ?*anyopaque) void {}
