#ifndef TOURNAMENTTREEH
#define TOURNAMENTTREEH

#include <tuple>
#include <queue>
#include <cstdint>
#include "kvpair.h"

using namespace std;

class TournamentNode{
    public:
        TournamentNode(int identifier);
        void buildTree(queue<TournamentNode*> leaves);
        void updateTree();;
        ~TournamentNode();

        int leafID;
        TournamentNode* lhs = nullptr;//always both nullptr or both not
        TournamentNode* rhs = nullptr;
        TournamentNode* parent = nullptr;
        KVPair* value;
        bool hasValue = false;

    private:
        void update();
};

#endif
