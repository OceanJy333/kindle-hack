# input: ELF file; output: shared-library dependencies; pos: ARM32 runtime packaging.
import struct


def needed(path):
    data = path.read_bytes()
    if data[:5] != b"\x7fELF\x01":
        raise ValueError(f"Expected ARM32 ELF: {path}")
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    assert header[2] == 40
    sections = [struct.unpack_from("<IIIIIIIIII", data, header[6] + i * header[11])
                for i in range(header[12])]
    names = data[sections[header[13]][4]:sections[header[13]][4] + sections[header[13]][5]]
    sections = {names[s[0]:names.find(b"\0", s[0])].decode(): s for s in sections}
    def content(name):
        s = sections[name]
        return data[s[4]:s[4] + s[5]]
    strings = content(".dynstr")
    dynamic = content(".dynamic")
    dependencies = []
    for i in range(0, len(dynamic), 8):
        tag, offset = struct.unpack_from("<iI", dynamic, i)
        if tag == 1:
            dependencies.append(strings[offset:strings.find(b"\0", offset)].decode())
    return dependencies
