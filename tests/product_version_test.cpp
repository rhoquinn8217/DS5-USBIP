// The product's version: the string everything shows, and the numbers the exe's
// version resource is stamped with, must say the same thing.
//
// ⓘ A mismatch would not fail anywhere else. The window title and --version
// read the string, Explorer's Details tab reads the numbers, and each would look
// right on its own.

#include "harness.h"

#include <string>

#include "ctm/product.h"

using namespace ctmtest;

int run_product_version_tests()
{
    section("product version: the string matches the numbers");
    {
        const std::string fromNumbers = std::to_string(PRODUCT_VERSION_MAJOR) + "." +
                                        std::to_string(PRODUCT_VERSION_MINOR) + "." +
                                        std::to_string(PRODUCT_VERSION_PATCH);
        CTM_CHECK(fromNumbers == PRODUCT_VERSION);
    }

    section("product version: the name is ours, not the relay's");
    {
        CTM_CHECK(std::string(PRODUCT_NAME) == "DS5-USBIP");
        // ⓘ 0.x until a stranger can use it unaided (include/ctm/product.h).
        CTM_CHECK(PRODUCT_VERSION_MAJOR == 0);
    }

    section("product version: shown with the build number, as aurora-tv shows its own");
    {
        // ⓘ include/ctm/build_number.h, rewritten by build-number.ps1 for every
        // build. The window title, the tray tip, --version, the log's start line
        // and the exe's product version string all show this one form.
        const std::string display = std::string(PRODUCT_VERSION) + " (" + std::to_string(PRODUCT_BUILD) + ")";
        CTM_CHECK(display == PRODUCT_VERSION_DISPLAY);
        // ⓘ The exe's numeric product version keeps each of its four numbers in
        // 16 bits, the build being the fourth.
        CTM_CHECK(PRODUCT_BUILD >= 0 && PRODUCT_BUILD <= 65535);
    }
    return 0;
}
