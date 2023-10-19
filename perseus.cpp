#include <iostream>
#include "cube.h"

using namespace std;

int main() {

    Cube cube (5, 10);

    cout << "Cube min: " << cube.getMin() << endl;
    cout << "Cube max: " << cube.getMax() << endl;

    return 0;

}