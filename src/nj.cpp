#include "nj.h"
#include "tree.h"
#include "debug.h"
#include <vector>
using std::vector;
#include <string>
using std::string;
#include <queue>
using std::queue;
#include <stack>
using std::stack;
#include <unordered_map>
using std::unordered_map;
#include <utility>
#include <cmath>

#include <iostream>

nj_t::nj_t(const vector<float>& dists, const vector<string>& labels){
    debug_string("");
    _dists = dists;
    //because dists is a square matrix, we need to compute the row size.
    //furthemore, since it is a square matrix, the sqrt of the size should an integer
    //there might be some round off, but I tested this on a skylake up to 2^30ish
    //so any number that I would need to calculate this for should work
    _row_size = sqrt(dists.size());
    _tree.resize(_row_size);
    debug_print("assigning labels to nodes, row_size: %lu, lables.size(): %lu", _row_size, labels.size());
    for(size_t i=0;i<_row_size;++i){
        _tree[i] = new node_t;
        debug_print("i: %lu, _tree[i] : %p", i, _tree[i]);
        _tree[i]->_label = labels[i];
    }

    debug_string("starting to join pairs");
    while(_row_size>3){
        join_pair();
    }
    debug_string("done joining");
    join_final();
    make_tree();
}

tree_t nj_t::get_tree(){
    return _final_tree;
}

void nj_t::compute_r(){
    debug_string("");

    debug_string("resizing the _r_vec");
    _r_vec.resize(_row_size, 0.0);
    debug_string("computing r");
    debug_print("_dists.size() : %lu, _row_size: %lu", _dists.size(), _row_size);
    for(size_t i=0;i<_row_size;++i){
        for(size_t j=0;j<_row_size;++j){
            debug_print("i*_row_size+j: %lu",i*_row_size+j);
            _r_vec[i] += _dists[i*_row_size+j];
        }
    }
}

void nj_t::compute_q(){
    debug_string("");
    //need the r_vec to compute q_vec, so make sure its updated
    compute_r();

    //now to compute the new matrix of values to determine cherry picking
    debug_string("resizing q");
    _q_vec.resize(_row_size*_row_size, 0.0);
    debug_string("computing q");
    for(size_t i=0;i<_row_size;++i){
        for(size_t j=0;j<_row_size;++j){
            _q_vec[i*_row_size+j] = (_row_size-2)*_dists[i*_row_size+j] - _r_vec[i] - _r_vec[j];
        }
    }
}

void nj_t::find_pair(){
    debug_string("");
    //compute the matrix Q, which is put into a private data member
    compute_q();
    debug_string("done computing q");

    //find the smallest entry in Q
    //that i,j is the pair we join
    //the way this loop is structured, i>j
    size_t low_i = 0;
    size_t low_j = 1;
    for(size_t i=0;i<_row_size;++i){
        for(size_t j=0;j<i;++j){
            if(i==j) continue;
            if(_q_vec[i*_row_size+j] <= _q_vec[low_i*_row_size+low_j]){
                low_i = i;
                low_j = j;
            }
        }
    }
    _i = low_i;
    _j = low_j;
}

//in this funciton, I modify the size of _tree and _dists.
//furthermore, _row_size's value changes to keep up.
void nj_t::join_pair(){
    debug_string("");
    find_pair();
    debug_print("_i:%lu, _j:%lu", _i,_j);
    //make a temp vector
    std::vector<node_t*> tmp_tree;
    for(size_t i=0;i<_row_size;++i){
        if(i==_i || i==_j) continue;
        tmp_tree.push_back(_tree[i]);
    }
    //join nodes and push onto the vector
    debug_string("joining nodes and pushing onto the tmp vector");


    node_t* tmp = new node_t;

    debug_print("new node pointer: %p", tmp);
    tmp->_lchild = _tree[_i];
    tmp->_rchild = _tree[_j];
    assert_string(tmp->_lchild && tmp->_rchild, "both children are not set");
    tmp->_children = true;
    _tree[_i]->_parent = tmp;
    _tree[_j]->_parent = tmp;
    tmp_tree.push_back(tmp);
    std::swap(_tree,tmp_tree);


    //need to calculate new distances for the new node
    //equation boosted from https://en.wikipedia.org/wiki/Neighbor_joining
    //actually, could be simplier
    tmp->_lchild->_weight = .5*_dists[_i*_row_size+_j] + 1/(2*(2*_row_size-2)) * 
        (_r_vec[_i] - _r_vec[_j]);

    tmp->_rchild->_weight = _dists[_i*_row_size+_j] - tmp->_lchild->_weight;

    //integrate the new node into the distance table
    debug_string("making tmp_dists");
    vector<float> tmp_dists((_row_size-1)*(_row_size-1), 0.0);
    size_t tmp_row_size = _row_size-1;

    for(size_t i=0;i<_row_size;++i){
        if(i==_i || i==_j) continue;
        size_t cur_i = i;
        if(i>_i) cur_i--;
        if(i>_j) cur_i--;
        for(size_t j=0;j<_row_size;++j){
            if(j==_j || j==_i) continue;
            size_t cur_j = j;
            if(j>_j) cur_j--;
            if(j>_i) cur_j--;
            debug_print("mapping (%lu, %lu) to (%lu, %lu)",i,j,cur_i,cur_j);
            tmp_dists[cur_i*tmp_row_size+cur_j] = _dists[i*_row_size+j];
        }
    }
    
    //reflect the matrix

    for(size_t i=0;i<tmp_row_size;++i){
        tmp_dists[i*tmp_row_size + (tmp_row_size-1)] = .5 * 
            (_dists[i*_row_size+_i] + _dists[i*_row_size+_j] - _dists[_i*_row_size + _j]);
        tmp_dists[tmp_row_size*(tmp_row_size-1) + i] = tmp_dists[i*tmp_row_size + (tmp_row_size-1)];
    }

    debug_string("swapping tmp_dists and _dists");
    _dists.swap(tmp_dists);
    _row_size--;
}

void nj_t::join_final(){
    debug_string("");
    assert_string(_row_size == 3, "the row size is wrong for the final join");

    /**
     * join the last 3
     *  to do that we need to use the three distance formulas 
     *    for a graph like
     *        x
     *        |
     *        r
     *       / \
     *      y   z
     *  We can calculate the x-r (d_xr) distance by calculating the following
     *     d_xr = (d_yx + d_yx - d_yz)/2
     *  and we can calculate the other d_ir for i in {x,y,z} the same way 
     */
    
    /*
    _tree[0]->_weight = .5* (_dists[0*_row_size+1] + _dists[0*_row_size+2] - _dists[1*_row_size+2]);
    _tree[1]->_weight = .5* (_dists[1*_row_size+2] + _dists[0*_row_size+1] - _dists[0*_row_size+2]);
    _tree[2]->_weight = .5* (_dists[1*_row_size+2] + _dists[2*_row_size+0] - _dists[0*_row_size+1]);
    */

    for(size_t i=0;i<_row_size;++i){
        size_t x,y,z;
        x = i;
        y = (i+1)%_row_size;
        z = (i+2)%_row_size;
        _tree[i]->_weight = (_dists[x*_row_size + y] + _dists[x*_row_size + z] - _dists[y*_row_size+z]);
        _tree[i]->_weight *= .5;
        debug_print("setting last weight to : .5* (%f + %f - %f) = %f",
            _dists[x*_row_size + y] , _dists[x*_row_size + z] , _dists[y*_row_size+z],
            _tree[i]->_weight);
    }
}

void nj_t::make_tree(){
    debug_string("");
    _final_tree = tree_t(_tree);
}

/*
void nj_t::flatten_tree(){
    debug_string("");
    //need a stack and a queue to flatten this tree
    std::queue<node_t*> node_q;
    std::stack<node_t*> node_stack;

    _unroot = _tree;

    debug_print("_tree.size(): %lu", _tree.size());
    
    for(auto n : _tree){
        node_stack.push(n);
        node_q.push(n);
    }

    while(!node_stack.empty()){
        auto tmp_node = node_stack.top(); node_stack.pop();
        debug_print("tmp_node->_children %i", tmp_node->_children);
        if(tmp_node->_children){
            node_stack.push(tmp_node->_lchild);
            node_stack.push(tmp_node->_rchild);
            node_q.push(tmp_node->_lchild);
            node_q.push(tmp_node->_rchild);
        }
    }

    _tree_size = node_q.size();
    _flat_tree = new node_t[_tree_size];
    for(size_t i=0;i<_tree_size;++i){
        _flat_tree[i] = node_t();
    }

    node_map nm;

    for(size_t i=0;i<_tree_size;++i){
        debug_print("current temp node weight: %f", node_q.front()->_weight);
        _flat_tree[i] = *node_q.front();
        nm[node_q.front()] = _flat_tree+i;
        node_q.pop();
    }

    for(size_t i=0;i<_tree_size;++i){
        debug_print("checking nm for %p", _flat_tree[i]._parent);
        debug_print("checking weight: %f", _flat_tree[i]._weight);
        if(_flat_tree[i]._parent != NULL)
            _flat_tree[i]._parent = nm.at(_flat_tree[i]._parent);
        if(_flat_tree[i]._lchild && _flat_tree[i]._rchild){
            debug_print("checking nm for children: %p", _flat_tree[i]._lchild);
            _flat_tree[i]._lchild = nm.at(_flat_tree[i]._lchild);
            _flat_tree[i]._rchild = nm.at(_flat_tree[i]._rchild);
        }
    }
    //update the unroot
    for(size_t i=0;i<_unroot.size();++i){
        _unroot[i] = nm.at(_unroot[i]);
        debug_print("unroot weight: %f", _unroot[i]->_weight);
    }
}
*/

void delete_node(node_t* n){
    debug_string("");
    if(n->_children){
        delete_node(n->_lchild);
        delete_node(n->_rchild);
    }
    delete n;
}

void nj_t::clean_up(){
    debug_string("");
    for(auto n : _tree){
        delete_node(n);
    }
}