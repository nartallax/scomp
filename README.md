# SComp - structured data compression

A streaming data compression algorithm that is able to recognize structured data in the stream and perform format-specific compressions over that portion of the data.  

## Development

This project is built with `clang`. It is probably buildable with other compilers, I didn't use anything compiler-specific, but also didn't bother to test anything but clang.  
This project also occasionally uses `llvm` tools, for code coverage.  

This will get you all the tools you need to build and test the project:  
```bash
sudo apt install llvm clang
```

After installing all the tooling, use `make` to run the commands. `make build`, `make test`, `make coverage` etc.  