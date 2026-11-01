#include "../src/uk_postcode.h"
#include <cassert>
#include <iostream>
int main() {
 using namespace UkPostcode;
 assert(format("ng12 3ab")=="NG12 3AB");
 assert(format("ng123ab")=="NG12 3AB");
 assert(format("sw1a1aa")=="SW1A 1AA");
 assert(format("W1A 0AX")=="W1A 0AX");
 assert(format("M1 1AE")=="M1 1AE");
 assert(format("EC1A1BB")=="EC1A 1BB");
 assert(format("GIR 0AA")=="GIR 0AA");
 assert(format("ng12")=="");
 assert(format("12345")=="");
 assert(format("N?12 3AB")=="");
 assert(format("AA9999A")=="");
 assert(format("AA9A 9AA")=="AA9A 9AA");
 assert(format("xx99 9xx")=="XX99 9XX"); // Syntax only, API verifies existence
 std::cout<<"UK postcode format and validation passed\n";
}
