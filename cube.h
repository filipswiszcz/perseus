#ifndef CUBE_H
#define CUBE_H

class Cube {

    private:

        int min, max;

    public:
    
        Cube(int, int);

        int getMin() {
            return min;
        }

        int getMax() {
            return max;
        }

};

#endif
