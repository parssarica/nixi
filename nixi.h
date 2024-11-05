#include <archive.h>
#include <gpgme.h>

void extract(const char *filename, int do_extract, int flags);
void warn(const char *f, const char *m);
void fail(const char *f, const char *m, int r);
int copy_data(struct archive *ar, struct archive *aw);
void errmsg(const char *m);
void check_error(gpgme_error_t err);
int check_sig(char* sigfile, char* datfile);
