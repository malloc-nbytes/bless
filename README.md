# bless

"Better" `less` is a part of the series that I am doing to rewrite
some of the GNU coreutils. It aims to be the exact same as `less` except
with some quality of life features.

`Bless` can take any number of files (even none) and will open them as
separate buffers. You can scroll, tab through them, search, and more. You can
even save buffers as well as the line number for later use.

If no files are given, or if you type `?`, it will open the internal
usage buffer which has all the commands that you can perform. You can
also do `O` to open a file from within `Bless`.

To open your saved buffers, you can do `C-o` and to save a buffer do `C-w`.
