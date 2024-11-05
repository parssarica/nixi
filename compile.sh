#gcc -o nixi nixi.c untar.c -fsanitize=address -static-libasan -g -Wall -Wextra -pedantic -O2 -lsqlite3 -lcurl -larchive
gcc -o nixi nixi.c untar.c gpg.c -ggdb3 -O0 -Wall -Wextra -pedantic -lsqlite3 -lcurl -larchive -Wstrict-prototypes -lgpgme
#clang -o nixi nixi.c untar.c -ggdb3 -D_FORTIFY_SOURCE=3 -Wall -Wextra -pedantic -O2 -lsqlite3 -lcurl -larchive
#gcc -o nixi nixi.c untar.c -D_FORTIFY_SOURCE=3 -Wall -Wextra -pedantic -O2 -lsqlite3 -lcurl -larchive
