#include "dlss_capture_pair.h"
#include <cassert>
int main(){
    assert(dlsscapture::validName(L"NR_CAPTURE_1793945087999_8.meta.txt"));
    assert(!dlsscapture::validName(L"NR_CAPTURE_../evil.meta.txt"));
    assert(!dlsscapture::validName(L"NR_CAPTURE_1.png"));
    assert(dlsscapture::validMetadata("comparison=final-present-same-frame\r\n"));
    assert(!dlsscapture::validMetadata("comparison=model-buffers\n"));
    return 0;
}
