package = "buffer"
version = "scm-1"
source = {
    url = "git://github.com/mah0x211/lua-buffer.git"
}
description = {
    summary = "buffer module",
    homepage = "https://github.com/mah0x211/lua-buffer",
    license = "MIT",
    maintainer = "Masatoshi Teruya"
}
dependencies = {
    "lua >= 5.1"
}
build = {
    type = "builtin",
    modules = {
        buffer = {
            sources = { "src/buffer.c" }
        }
    }
}

