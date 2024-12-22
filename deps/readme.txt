
Originally I kept the dependencies in ${LGI}/../../../codelib but I think that's too far
away from the LGI clone location. So in making things more compact, I'm moving them to
${LGI}/../deps and making scripts to check out and build them in this folder.

${LGI}/deps/build.py
    Will checkout the libraries and build them such that LGI can use them for the current
    OS and config (Debug/Release).

${LGI}/deps/CMakeLists.txt
    Defines targets for the various dependencies that LGI uses. Will point to the headers
    and libs in ${LGI}/../deps

Main dependencies:
    - libpng
    - zlib
    - libjpeg
    - lunasvg
    - libntlm
    - libiconv