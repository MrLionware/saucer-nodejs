#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "export.h"

#include <saucer/webview.h>
#include <stdint.h>

    enum SAUCER_LAYOUT
    {
        SAUCER_LAYOUT_PORTRAIT,
        SAUCER_LAYOUT_LANDSCAPE,
    };

    struct saucer_print_settings;
    typedef struct saucer_print_settings saucer_pdf_settings;

    enum saucer_pdf_layout : uint8_t
    {
        SAUCER_PDF_LAYOUT_PORTRAIT,
        SAUCER_PDF_LAYOUT_LANDSCAPE,
    };

    SAUCER_PDF_EXPORT saucer_print_settings *saucer_print_settings_new();
    SAUCER_PDF_EXPORT void saucer_print_settings_free(saucer_print_settings *);

    SAUCER_PDF_EXPORT void saucer_print_settings_set_file(saucer_print_settings *, const char *file);
    SAUCER_PDF_EXPORT void saucer_print_settings_set_orientation(saucer_print_settings *, SAUCER_LAYOUT orientation);

    SAUCER_PDF_EXPORT void saucer_print_settings_set_width(saucer_print_settings *, double width);
    SAUCER_PDF_EXPORT void saucer_print_settings_set_height(saucer_print_settings *, double height);

    SAUCER_PDF_EXPORT void saucer_pdf_settings_free(saucer_pdf_settings *);
    SAUCER_PDF_EXPORT saucer_pdf_settings *saucer_pdf_settings_new(const char *path);
    SAUCER_PDF_EXPORT void saucer_pdf_settings_set_size(saucer_pdf_settings *, double w, double h);
    SAUCER_PDF_EXPORT void saucer_pdf_settings_set_orientation(saucer_pdf_settings *, saucer_pdf_layout);

    struct saucer_pdf;

    SAUCER_PDF_EXPORT saucer_pdf *saucer_pdf_new(saucer_handle *webview);
    SAUCER_PDF_EXPORT void saucer_pdf_free(saucer_pdf *);

    SAUCER_PDF_EXPORT void saucer_pdf_save(saucer_pdf *, saucer_print_settings *settings);

#ifdef __cplusplus
}
#endif
