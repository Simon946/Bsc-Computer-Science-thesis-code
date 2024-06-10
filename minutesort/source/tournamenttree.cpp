#include <cstdint>
#include <tuple>
#include <queue>
#include <string.h>
#include <assert.h>

#include "include/tournamenttree.h"
#include "include/sorting.h"

using namespace std;

TournamentNode::TournamentNode(int identifier){
    leafID = identifier;
}

void TournamentNode::buildTree(queue<TournamentNode*> leaves){
    assert(leaves.size() > 1);
    assert(lhs == nullptr && rhs == nullptr);

    while(leaves.size() > 2){
        TournamentNode* newParent = new TournamentNode(0);
        newParent->lhs = leaves.front();
        leaves.pop();
        newParent->rhs = leaves.front();
        leaves.pop();
        
        newParent->lhs->parent = newParent;
        newParent->rhs->parent = newParent;
        newParent->update();
        leaves.push(newParent);
    }
    lhs = leaves.front();
    rhs = leaves.back();
    lhs->parent = this;
    rhs->parent = this;
    this->update();
}

void TournamentNode::update(){

    if(lhs == nullptr || rhs == nullptr){
        return;
    }
    TournamentNode* winningChild = lhsSmallerRhs(*lhs->value, *rhs->value)? lhs : rhs;
    TournamentNode* losingChild = (winningChild == lhs)? rhs : lhs;

    if(!winningChild->hasValue){
        winningChild = losingChild;
    }
    leafID = winningChild->leafID;
    hasValue = winningChild->hasValue;

    if(hasValue){
        value = winningChild->value;
    }
}

void TournamentNode::updateTree(){

    TournamentNode* current = this;
    do{
        current->update();
        current = current->parent;
    }while(current != nullptr);
}

TournamentNode::~TournamentNode(){
    delete lhs;
    delete rhs;
}
