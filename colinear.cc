#include "container.hpp"

struct point_t
{
    int x, y;
};

bool comp_point_t(const point_t &p1, const point_t &p2)
{
    return 0;
}

bool less_point_t(const point_t &p1, const point_t &p2)
{
    return comp_point_t(p1, p2) < 0;
}

struct fraction_t
{
    int numerater, denumerator;

    fraction_t(int numerater, denumerator)
        : numerater(numerater),
          denumerator(denumerator) {
              simplify();
          }
private:
    void simplify()
    {
        int gcd = get_gcd(numerater, denumerator);
        numerator /= gcd;
        denumerator /= gcd;
    }
};

bool less_fraction_t(const fraction_t &f1, const fraction_t &f2)
{
    return 0;
}

static int get_y_intersection(const fraction_t& f, const point_t& x)
{
    return 0;
}

struct line_t
{
    line_t(const point& x, const point& y)
    slope(x.y-y.y, x.x-y.x),
    y_intersection(get_y_intersection(slope, x)) {
    }

    int y_intersection;
    fraction_t slope;

    // store the index rather than the point!
    set<point_t> ps;

    line_t &add(const point_t &p)
    {
        return *this;
    }
};

bool less_line_t(const line_t &line1, const line_t &line2)
{
    return false;
}

void colinear(vector<point_t> ps)
{
    std::sort(ps, less_point_t);

    set<line_t, less_line_t> mem;
    foreach(i, ps) {
        LET(j, i + 1);
        while(j != ps.end()) {
            mem[line_t(*i, *j)].add(*i).add(*j);
        }
    }
}

// Another thought: 
// suppose (x, x^3), (y, y^3), (z, z^3) are on the same line.
// then compare the slope of <x, y> and <x, z> will get x + y + z = 0
// But this can only be used as a proof.

void points_on_line()   
{
    const int dirs =360;            // num of directions (accuracy)
    double mdir=double(dirs)/M_PI;  // conversion from angle to code
    double pacc=0.01;               // position acc <0,1>
    double lmin=0.05;               // min line size acc <0,1>
    double lmax=0.25;               // max line size acc <0,1>
    double pacc2,lmin2,lmax2;

    int n,ia,ib;
    double x0,x1,y0,y1;
    struct _lin
    {
        int dir;        // dir code <0,dirs>
        double ang;     // dir [rad] <0,M_PI>
        double dx,dy;   // dir unit vector
        int i0,i1;      // index of points
    } *lin;
    glview2D::_pnt *a,*b;
    glview2D::_lin q;
    _lin l;
    // prepare buffers
    n=view.pnt.num;     // n=number of points
    n=((n*n)-n)>>1;     // n=max number of lines
    lin=new _lin[n]; n=0;
    if (lin==NULL) return;
    // precompute size of area and update accuracy constants ~O(N)
    for (a=view.pnt.dat,ia=0;ia<view.pnt.num;ia++,a++)
    {
        if (!ia)
        {
            x0=a->p[0]; y0=a->p[1];
            x1=a->p[0]; y1=a->p[1];
        }
        if (x0>a->p[0]) x0=a->p[0];
        if (x1<a->p[0]) x1=a->p[0];
        if (y0>a->p[1]) y0=a->p[1];
        if (y1<a->p[1]) y1=a->p[1];
    }
    x1-=x0; y1-=y0; if (x1>y1) x1=y1;
    pacc*=x1; pacc2=pacc*pacc;
    lmin*=x1; lmin2=lmin*lmin;
    lmax*=x1; lmax2=lmax*lmax;
    // precompute lines ~O((N^2)/2)
    for (a=view.pnt.dat,ia=0;ia<view.pnt.num;ia++,a++)
        for (b=a+1,ib=ia+1;ib<view.pnt.num;ib++,b++)
        {
            l.i0=ia;
            l.i1=ib;
            x0=b->p[0]-a->p[0];
            y0=b->p[1]-a->p[1];
            x1=(x0*x0)+(y0*y0);
            if (x1<=lmin2) continue;        // ignore too small lines
            if (x1>=lmax2) continue;        // ignore too big lines
            l.ang=atanxy(x0,y0);
            if (l.ang>M_PI) l.ang-=M_PI;    // 180 deg is enough lines goes both ways ...
            l.dx=cos(l.ang);
            l.dy=sin(l.ang);
            l.dir=double(l.ang*mdir);
            lin[n]=l; n++;
            //      q.p0=*a; q.p1=*b; view.lin.add(q);  // just visualise used lines for testing
        }

    // test directions
    int cnt,cntmax=0;
    double t;
    for (ia=0;ia<n;ia++)
    {
        cnt=1;
        for (ib=ia+1;ib<n;ib++)
            if (lin[ia].dir==lin[ib].dir)
            {
                a=&view.pnt[lin[ia].i0];
                if (lin[ia].i0!=lin[ib].i0)
                    b=&view.pnt[lin[ib].i0];
                else b=&view.pnt[lin[ib].i1];
                x0=b->p[0]-a->p[0]; x0*=x0;
                y0=b->p[1]-a->p[1]; y0*=y0;
                t=sqrt(x0+y0);
                x0=a->p[0]+(t*lin[ia].dx)-b->p[0]; x0*=x0;
                y0=a->p[1]+(t*lin[ia].dy)-b->p[1]; y0*=y0;
                t=x0+y0;
                if (fabs(t)<=pacc2) cnt++;
            }
        if (cntmax<cnt)                         // if more points on single line found
        {
            cntmax=cnt;                         // update point count
            q.p0=view.pnt[lin[ia].i0];          // copy start/end point
            q.p1=q.p0;
            q.p0.p[0]-=500.0*lin[ia].dx;    // and set result line as very big (infinite) line
            q.p0.p[1]-=500.0*lin[ia].dy;
            q.p1.p[0]+=500.0*lin[ia].dx;
            q.p1.p[1]+=500.0*lin[ia].dy;
        }
    }
    if (cntmax) view.lin.add(q);

    view.redraw=true;
    delete lin;
    //  Caption=n;      // just to see how many lines actualy survive the filtering
}
