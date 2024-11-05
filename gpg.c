#include <gpgme.h>
#include <stdio.h>
#include <stdlib.h>

void check_error(gpgme_error_t err) {
    if (err) {
        fprintf(stderr, "GPGME error: %s\n", gpgme_strerror(err));
        exit(1);
    }
}

int check_sig(char* sigfile, char* datfile)
{
    gpgme_ctx_t ctx;
    gpgme_error_t err;
    gpgme_data_t sig, data;
    gpgme_verify_result_t result;
    int verify_result;

    // Initialize GPGME
    gpgme_check_version(NULL);
    err = gpgme_new(&ctx);
    check_error(err);

    // Set the context to use OpenPGP
    gpgme_set_protocol(ctx, GPGME_PROTOCOL_OpenPGP);

    // Open the signature and data files
    err = gpgme_data_new_from_file(&sig, sigfile, 1); // Signature file
    check_error(err);
    err = gpgme_data_new_from_file(&data, datfile, 1); // Data file
    check_error(err);

    // Verify the signature
    err = gpgme_op_verify(ctx, sig, data, NULL);
    check_error(err);

    // Get the result
    result = gpgme_op_verify_result(ctx);
    if (result->signatures && result->signatures->status == GPG_ERR_NO_ERROR) {
        verify_result = 1;
    } else {
        verify_result = 0;
        gpgme_strerror(result->signatures->status);
    }

    // Clean up
    gpgme_data_release(sig);
    gpgme_data_release(data);
    gpgme_release(ctx);

    return verify_result;
}

/*
int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <signature_file> <data_file>\n", argv[0]);
        return 1;
    }

    gpgme_ctx_t ctx;
    gpgme_error_t err;
    gpgme_data_t sig, data;
    gpgme_verify_result_t result;

    // Initialize GPGME
    gpgme_check_version(NULL);
    err = gpgme_new(&ctx);
    check_error(err);

    // Set the context to use OpenPGP
    gpgme_set_protocol(ctx, GPGME_PROTOCOL_OpenPGP);

    // Open the signature and data files
    err = gpgme_data_new_from_file(&sig, argv[1], 1); // Signature file
    check_error(err);
    err = gpgme_data_new_from_file(&data, argv[2], 1); // Data file
    check_error(err);

    // Verify the signature
    err = gpgme_op_verify(ctx, sig, data, NULL);
    check_error(err);

    // Get the result
    result = gpgme_op_verify_result(ctx);
    if (result->signatures && result->signatures->status == GPG_ERR_NO_ERROR) {
        printf("Signature is valid.\n");
    } else {
        printf("Signature verification failed: %s\n",
               gpgme_strerror(result->signatures->status));
    }

    // Clean up
    gpgme_data_release(sig);
    gpgme_data_release(data);
    gpgme_release(ctx);

    return 0;
}
*/
