"""Markdown tables shared by parser and HTTP storage reports."""


def render(title: str | None, header: list[str], rows: list[list[str]], level: int = 3) -> None:
    values = [header, *rows]
    widths = [max(3, *(len(row[i]) for row in values))
              for i in range(len(header))]
    if title is not None:
        print("\n\n" + "#" * level + f" {title}\n")
    else:
        print()
    values.insert(1, ["-" * width for width in widths])
    for row in values:
        print("| " + " | ".join(cell.ljust(width)
                               for cell, width in zip(row, widths)) + " |")


def environment_block(path):
    """Format common environment metadata, excluding adapter build details."""
    if not path.exists():
        return ""
    rows = [line.strip().split(": ", 1) for line in path.read_text().splitlines()
            if ": " in line
            and line.split(": ", 1)[0] not in {"compiler", "cflags", "cxx", "cxxflags"}
            and not line.split(": ", 1)[0].endswith((" build", " variants"))]
    width = max((len(row[0]) for row in rows), default=0)
    values = "\n".join(f"{key:<{width}} : {value}" for key, value in rows)
    return "## Environment\n\n```text\n" + values + "\n```"


def environment(path):
    block = environment_block(path)
    if block:
        print("\n\n" + block)


def update_environment(text, path):
    """Keep generated metadata immediately after the Benchmark heading."""
    import re
    if path.exists():
        metadata = dict(line.split(": ", 1) for line in path.read_text().splitlines()
                        if ": " in line)
        for key, value in metadata.items():
            if key.endswith(" build"):
                owner = key[:-len(" build")]
                compiler, separator, details = value.partition("; ")
                metadata_key = "compiler:" + owner
                marker = "<!-- " + metadata_key + " -->"
                text = re.sub(re.escape(marker) + r".*?<!-- /compiler -->",
                              lambda _: marker + "`" + compiler + "`<!-- /compiler -->",
                              text, flags=re.DOTALL)
                marker = "<!-- build:" + owner + " -->"
                text = re.sub(re.escape(marker) + r".*?<!-- /build -->",
                              lambda _: marker + (details if separator else value) + "<!-- /build -->",
                              text, flags=re.DOTALL)
        for field, keys in (("compiler", ("compiler", "cxx")),
                            ("flags", ("cflags", "cxxflags"))):
            for key in keys:
                if key not in metadata:
                    continue
                marker = "<!-- " + field + ":" + key + " -->"
                text = re.sub(re.escape(marker) + r".*?<!-- /" + field + r" -->",
                              lambda _: marker + "`" + metadata[key] + "`<!-- /" + field + " -->",
                              text, flags=re.DOTALL)
    block = environment_block(path)
    if not block:
        return text
    start = "<!-- benchmark-environment -->"
    end = "<!-- /benchmark-environment -->"
    section = start + "\n" + block + "\n" + end
    if start in text:
        return re.sub(re.escape(start) + r".*?" + re.escape(end),
                      lambda _: section, text, count=1, flags=re.DOTALL)
    return text.replace("# Benchmark\n", "# Benchmark\n\n" + section + "\n", 1)


def message_section(name):
    """Display the measured fixture, normalizing CRLF for Markdown only."""
    from gen_fixtures import fixture_info
    fixture = fixture_info(name)
    message = fixture["message"]
    print(f"\n\n### {fixture['title']}\n")
    if fixture["description"]:
        print(fixture["description"] + "\n")
    print(f"<details>\n<summary>Message ({len(message.encode('ascii'))} bytes)</summary>\n")
    print("```http\n" + message.replace("\r\n", "\n") + "```\n")
    print("</details>")
