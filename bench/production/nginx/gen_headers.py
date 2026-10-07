#!/usr/bin/env python3
"""Extract native known-header offsets, retaining upstream feature guards."""
import re
from pathlib import Path
base = Path(__file__).resolve().parent
source = (base / "deps/src/http/ngx_http_request.c").read_text()
source = source.split("ngx_http_headers_in[] = {", 1)[1].split("};", 1)[0]
pattern = r'(?m)^#(?:if|else|endif)[^\n]*|\{ ngx_string\("([^\"]+)"\),\s*(offsetof\(ngx_http_headers_in_t,\s*\w+\)|0),'
lines = ["/* Generated from nginx's native known-header table. */", "static ngx_hash_key_t native_header_keys[] = {"]
for match in re.finditer(pattern, source):
    if match[0].startswith("#"):
        lines.append(match[0])
    elif match[2] != "0":
        lines.append(f'    {{ ngx_string("{match[1].lower()}"), 0, (void *)(uintptr_t)({match[2]} + 1) }},')
lines.append("};")
text = "\n".join(lines) + "\n"
path = base / "bin/header_keys.h"
path.parent.mkdir(exist_ok=True)
if not path.exists() or path.read_text() != text:
    path.write_text(text)
