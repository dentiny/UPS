#ifndef PACKAGE_HPP__  
#define PACKAGE_HPP__

class Package
{
public:
    Package() : 
        packageid { -1 },
        dest_x { -1 },
        dest_y { -1 }
        {}

    Package(int _packageid, int _dest_x, int _dest_y) :
        packageid { _packageid },
        dest_x { _dest_x },
        dest_y { _dest_y }
        {}

    int getPackageid() const { return packageid; }

    int getDestX() const { return dest_x; }

    int getDestY() const { return dest_y; }

private:
    int packageid;
    int dest_x;
    int dest_y;
};

#endif