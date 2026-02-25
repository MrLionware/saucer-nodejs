#include "pdf.h"

#include "webview.hpp"
#include "utils/handle.hpp"

#include <saucer/modules/pdf.hpp>

struct saucer_print_settings : bindings::handle<saucer_print_settings, saucer::modules::pdf::settings>
{
};

using saucer_pdf_settings = saucer_print_settings;

struct saucer_pdf : bindings::handle<saucer_pdf, saucer::modules::pdf>
{
};

extern "C"
{
    saucer_print_settings *saucer_print_settings_new()
    {
        return saucer_print_settings::make();
    }

    void saucer_print_settings_free(saucer_print_settings *handle)
    {
        delete handle;
    }

    void saucer_print_settings_set_file(saucer_print_settings *handle, const char *file)
    {
        handle->value().file = file ? file : "";
    }

    void saucer_print_settings_set_orientation(saucer_print_settings *handle, SAUCER_LAYOUT orientation)
    {
        handle->value().orientation = orientation == SAUCER_LAYOUT_LANDSCAPE
            ? saucer::modules::pdf::layout::landscape
            : saucer::modules::pdf::layout::portrait;
    }

    void saucer_print_settings_set_width(saucer_print_settings *handle, double width)
    {
        handle->value().size.w = width;
    }

    void saucer_print_settings_set_height(saucer_print_settings *handle, double height)
    {
        handle->value().size.h = height;
    }

    void saucer_pdf_settings_free(saucer_pdf_settings *settings)
    {
        saucer_print_settings_free(settings);
    }

    saucer_pdf_settings *saucer_pdf_settings_new(const char *path)
    {
        auto *settings = saucer_print_settings_new();
        saucer_print_settings_set_file(settings, path ? path : "");
        return settings;
    }

    void saucer_pdf_settings_set_size(saucer_pdf_settings *settings, double w, double h)
    {
        saucer_print_settings_set_width(settings, w);
        saucer_print_settings_set_height(settings, h);
    }

    void saucer_pdf_settings_set_orientation(saucer_pdf_settings *settings, saucer_pdf_layout layout)
    {
        saucer_print_settings_set_orientation(
            settings, layout == SAUCER_PDF_LAYOUT_LANDSCAPE ? SAUCER_LAYOUT_LANDSCAPE : SAUCER_LAYOUT_PORTRAIT);
    }

    saucer_pdf *saucer_pdf_new(saucer_handle *webview)
    {
        return saucer_pdf::make(webview->view);
    }

    void saucer_pdf_free(saucer_pdf *handle)
    {
        delete handle;
    }

    void saucer_pdf_save(saucer_pdf *handle, saucer_print_settings *settings)
    {
        handle->value().save(settings->value());
    }
}
