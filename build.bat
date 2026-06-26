@echo off
REM build.bat — Compila todo o projeto (bibliotecas + ferramentas + testes)
REM Uso: build.bat

echo [1/4] Limpando build anterior...
rmdir /s /q build 2>nul
mkdir build

echo [2/4] Compilando bibliotecas...
gcc -std=c11 -Wall -Wextra -Werror -Isrc -c src/sha256.c     -o build/sha256.o     || goto erro
gcc -std=c11 -Wall -Wextra -Werror -Isrc -c src/hash_tree.c  -o build/hash_tree.o  || goto erro
gcc -std=c11 -Wall -Wextra -Werror -Isrc -c src/node_cache.c -o build/node_cache.o || goto erro
gcc -std=c11 -Wall -Wextra -Werror -Isrc -c src/verity.c     -o build/verity.o     || goto erro

echo [3/4] Compilando ferramentas...
gcc -std=c11 -Wall -Wextra -Werror -Isrc tools/mkverity.c     build/sha256.o build/hash_tree.o build/node_cache.o build/verity.o -o build/mkverity     || goto erro
gcc -std=c11 -Wall -Wextra -Werror -Isrc tools/verify_block.c build/sha256.o build/hash_tree.o build/node_cache.o build/verity.o -o build/verify_block || goto erro
gcc -std=c11 -Wall -Wextra -Werror -Isrc tools/corrupt.c      build/sha256.o build/hash_tree.o build/node_cache.o build/verity.o -o build/corrupt      || goto erro
gcc -std=c11 -Wall -Wextra -Werror -Isrc tools/bench.c        build/sha256.o build/hash_tree.o build/node_cache.o build/verity.o -o build/bench        || goto erro

echo [4/4] Compilando testes...
gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_sha256.c     build/sha256.o    -o build/test_sha256     || goto erro
gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_hash_tree.c  build/sha256.o build/hash_tree.o -o build/test_hash_tree  || goto erro
gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_node_cache.c build/sha256.o build/hash_tree.o build/node_cache.o -o build/test_node_cache || goto erro
gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_verity.c     build/sha256.o build/hash_tree.o build/node_cache.o build/verity.o -o build/test_verity || goto erro

echo.
echo === Build OK — zero warnings ===
echo   Ferramentas : build\mkverity, build\verify_block, build\corrupt, build\bench
echo   Testes      : build\test_sha256, build\test_hash_tree, build\test_node_cache, build\test_verity
goto fim

:erro
echo.
echo === ERRO na compilacao (codigo %ERRORLEVEL%) ===
exit /b 1

:fim
