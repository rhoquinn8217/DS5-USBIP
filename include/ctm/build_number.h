// DS5-USBIP's build number: which build of the program this is.
//
// rhoquinn8217, 2026-10-12: "we need a build number for DS5-USBIP like we have
// for aurora-tv". A version names a release, and many builds share one; this
// number tells them apart. product.h puts the two together as "0.1.0 (N)",
// aurora-tv's form, and everything that shows the version shows that.
//
// !! build-number.ps1 REWRITES THE NUMBER BELOW AND COMMITS IT AS "Build N"
// !! for every build, before anything compiles: the next number free across
// !! every branch and every build recorded on this PC. So never change it by
// !! hand (a committed number counts as used, and the next build steps past
// !! it), and keep it on its one line, digits only, or the script cannot read
// !! it. 0 is the number before any build took one.
//
// Include guards rather than #pragma once: the resource compiler reads this
// too (app/ctm-usbip.rc, through product.h).

#ifndef DS5_USBIP_BUILD_NUMBER_H
#define DS5_USBIP_BUILD_NUMBER_H

#define PRODUCT_BUILD 15

#endif
