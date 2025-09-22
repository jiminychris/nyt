clang code/main.c -o ../build/nyt
clang code/nyt_asset_packer.c -o ../build/nyt_asset_packer
../build/nyt_asset_packer ../build/nyt dist/scrabble.txt
