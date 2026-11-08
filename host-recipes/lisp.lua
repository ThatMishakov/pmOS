return function(c)
    return Tool {
        name = "lisp",
        version = "0",
        revision = 1,
        source = c.source("lisp"),
        dependencies = c.deps { "build-essential", "clang" },
        build = c.script([=[
cp -rv ${SOURCE_DIR}/* .

make -j${PARALLELISM}
        ]=]),
        install = ":",
    }
end
