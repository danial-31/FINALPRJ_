#include "collision.hpp"
#include <iostream>
#include <limits>
#include <queue>

int main() {
    int failures=0;
    auto check=[&](bool ok,const char* name) {
        if (!ok) {std::cerr << "FAIL: " << name << '\n'; ++failures;}
    };
    // Every rendered object's occupied cells must be unique and fully interior.
    for (size_t i=0;i<layout::footprints.size();++i) {
        const auto a=layout::footprints[i];
        check(a.col>0 && a.row>0 && a.col+a.width<layout::columns &&
              a.row+a.depth<layout::rows,"object inside playable grid");
        check(!canMove(layout::center(a)),"object center blocked");
        for (size_t j=i+1;j<layout::footprints.size();++j) {
            const auto b=layout::footprints[j];
            check(a.col+a.width<=b.col || b.col+b.width<=a.col ||
                  a.row+a.depth<=b.row || b.row+b.depth<=a.row,"object footprints do not overlap");
        }
    }
    int freeCells=0;
    for (int row=0;row<layout::rows;++row) for(int col=0;col<layout::columns;++col) {
        const bool blocked=layout::isBlocked(col,row);
        check(canMove(layout::center({col,row,1,1})) != blocked,"grid center agrees with continuous collision");
        if (!blocked) ++freeCells;
    }
    // All free cells must connect to the central combat space, including the corners.
    bool visited[layout::rows][layout::columns]{};
    std::queue<std::pair<int,int>> frontier;
    frontier.push({10,8}); visited[8][10]=true;
    int reached=0;
    constexpr int dx[]={1,-1,0,0}, dz[]={0,0,1,-1};
    while(!frontier.empty()) {
        const auto cell=frontier.front(); frontier.pop(); ++reached;
        for(int i=0;i<4;++i) {
            const int col=cell.first+dx[i],row=cell.second+dz[i];
            if(layout::isBlocked(col,row) || visited[row][col]) continue;
            // Check the whole transition, not only its endpoints.
            const auto a=layout::center({cell.first,cell.second,1,1});
            const auto b=layout::center({col,row,1,1});
            for(int step=1;step<=10;++step)
                check(canMove(a+(b-a)*(float(step)/10.0f)),"continuous route between neighboring cells");
            visited[row][col]=true; frontier.push({col,row});
        }
    }
    check(reached==freeCells,"all walkable cells connected");
    check(canMove({0,0,-5.15f}),"clear of altar by body radius");
    check(!canMove({0,0,-5.17f}),"body overlaps altar");
    check(canMove({3.12f,0,-5.28f}),"rounded altar corner clear");
    check(!canMove({3.10f,0,-5.30f}),"rounded altar corner overlaps");
    check(canMove({12.15f,0,0}),"inside right border");
    check(!canMove({12.17f,0,0}),"body overlaps right border");
    glm::vec3 sliding{0,0,-5.1f};
    for(int i=0;i<10;++i) {
        const auto next=sliding+glm::vec3(.1f,0,-.1f);
        if(canMove({next.x,0,sliding.z})) sliding.x=next.x;
        if(canMove({sliding.x,0,next.z})) sliding.z=next.z;
    }
    check(sliding.x>.99f && sliding.z>=-5.16f && canMove(sliding),"sliding along altar");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    check(!canMove({nan,0,0}) && !canMove({0,0,nan}) &&
          !canMove({inf,0,0}) && !canMove({0,0,-inf}),"invalid positions rejected");
    if(failures) return 1;
    std::cout << "Grid checks passed: 17 distinct footprints, " << freeCells
              << " connected walkable cells, continuous clearance and sliding.\n";
    return 0;
}
