#include "../src/photo_match.h"
#include <cassert>
#include <iostream>
int main() {
    using PhotoMatch::titleHasRegistration;
    assert(titleHasRegistration("File:G-EUUK British Airways A320.jpg", "G-EUUK"));
    assert(titleHasRegistration("File:g-euuk (2017).jpeg", "g-euuk"));
    assert(titleHasRegistration("File:A320 G-EUUK.jpg", "G-EUUK"));
    assert(!titleHasRegistration("File:G-EUUK2.jpg", "G-EUUK"));
    assert(!titleHasRegistration("File:ZG-EUUK.jpg", "G-EUUK"));
    assert(!titleHasRegistration("File:G-EUUK.jpg", "G-EUUX"));
    assert(!titleHasRegistration("File:Boeing 737.jpg", ""));
    std::cout << "Exact aircraft photo registration matching passed\n";
}
