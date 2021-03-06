//tree.cpp
//Ben Bettisworth
//A class that implements a phylogentic tree. Features of the class:
//  - Handles the newick parsing (via newick.cpp)
//  - Packed to take advantage of cache locallity
//  - Handles labels

#include "debug.h"
#include "tree.h"
#include "newick.h"

#include <string>
using std::string;

#include <iomanip>

#include <unordered_map>
using std::unordered_map;

#include<vector>
using std::vector;

#include <stack>
using std::stack;

#include <queue>
using std::queue;

#include <sstream>
using std::ostringstream;

#include <functional>
using std::function;

#include <cassert>
#include <iostream>

size_t node_t::count_nodes(){
    size_t children = 0;
    if(_children){
        children += _lchild->count_nodes();
        children += _rchild->count_nodes();
    }
    return children+1;
}

void node_t::update_children(const unordered_map<node_t*, node_t*> node_map,
        node_t* p){
    _parent = p;
    if(_children){
        _lchild = node_map.at(_lchild);
        _rchild = node_map.at(_rchild);
        _lchild->update_children(node_map, this);
        _rchild->update_children(node_map, this);
    }
}

void node_t::set_weights_constant(double c){
    _weight=c;
    if(_children){
        _lchild->set_weights_constant(c);
        _rchild->set_weights_constant(c);
    }
}

void node_t::set_weights(function<double(size_t)> w_func, size_t depth,
        double max){

    if(!_children){
        double total=0;
        for(size_t i = 0; i < depth; ++i){
            total+= w_func(i);
            debug_print("total: %f", total);
        }
        _weight = max - total;
    }
    else{
        _lchild->set_weights(w_func, depth+1, max);
        _rchild->set_weights(w_func, depth+1, max);
        _weight = w_func(depth);
    }
}

void node_t::set_weights_as_root(function<double(size_t)> w_func, size_t depth,
        double max){
    _weight=0.0;
    if(_children){
        _lchild->set_weights(w_func, depth, max);
        _rchild->set_weights(w_func, depth, max);
    }
}

string node_t::sort(){
    if(_children){
        auto lchild_string = _lchild->sort();
        auto rchild_string = _rchild->sort();
        if(rchild_string < lchild_string){
            std::swap(_lchild, _rchild);
            std::swap(lchild_string, rchild_string);
        }
        if(lchild_string < _label || _label.empty()){
            return lchild_string;
        }
    }
    return _label;
}

size_t node_t::calc_max_depth(){
    if(_children){
        size_t lchild_depth = _lchild->calc_max_depth();
        size_t rchild_depth = _rchild->calc_max_depth();
        return lchild_depth > rchild_depth ? lchild_depth+1 : rchild_depth+1;
    }
    return 1;
}

/*
 * Set root sets the root of the tree, based on the outgroup. This is after the
 * outgroup is found on the tree. The outgroup is assumed to be on the tree. We
 * want to make
 *              O
 *              |
 *              .
 *              .
 *              .
 *              |
 *              r
 *             / \
 *            B   C
 * Into
 *              p
 *             / \
 *            O   .
 *                .
 *                .
 *                |
 *                r
 *               / \
 *              A   B
 *
 * Where O is the outgroup, r is the old unroot, and A,B are subtrees. We want
 * to make the new node p which is the root of the whole tree. To do this, we
 * need to add a node to the tree, (the root). Since there is a reallocate and
 * copy, we might as well make a new flat tree using the unroot.
 *
 *              O
 *              |
 *              o
 *             / \
 *            A   B
 *
 * So, let O be the left child of o (if its not, make it so by swapping the
 * left and right child). This is guaranteed since O is a child, and so the
 * parent cannot be pointing towards O. We make a new node p, which has as its
 * children O, and the node o (which will need to be reoriented so that the
 * parent points "up"). Since O is the left child of O, we replace O with p as
 * o's left child, and swap the left child and the parent. This yeilds the tree
 * below
 *
 *              p
 *             / \
 *            O   o
 *               / \
 *              A   B
 *
 * Note that A and B might still have the wrong orientation, so we need to call
 * the recursive funciton swap_parent on them to reorient them to point to the
 * right root.
 *
 * TODO: refactor
 */
void tree_t::set_root(node_t* outgroup){
    debug_string("making a new node");
    debug_print("outgroup: %p", outgroup);
    debug_print("_unroot.size(): %lu", _unroot.size());

    if(outgroup->_parent==nullptr){
        debug_string("outgroup has no parent");
        node_t* tmp = new node_t;
        //find the two that aren't the outgroup
        size_t idx;
        for(idx = 0; idx < _unroot.size(); ++idx){
            if(_unroot[idx] == outgroup) break;
        }
        _unroot.erase(_unroot.begin()+idx);
        tmp->_lchild = _unroot[0];
        tmp->_rchild = _unroot[1];
        tmp->_children = true;
        tmp->_rchild->_parent = tmp;
        tmp->_lchild->_parent = tmp;

        _unroot.clear();
        _unroot.push_back(outgroup);
        _unroot.push_back(tmp);
        make_flat_tree(std::vector<node_t*>(_unroot));
        tmp->_lchild = nullptr;
        tmp->_rchild = nullptr;
        delete tmp;
        return;
    }

    node_t* ur = new node_t;

    ur->_parent = _unroot[0];
    ur->_lchild = _unroot[1];
    ur->_rchild = _unroot[2];
    debug_print("ur->_parent: %p, ur->_lchild: %p, ur->_rchild: %p", ur->_parent,
            ur->_lchild, ur->_rchild);

    debug_string("clearing the old unroot, and pushing back the outgroup");
    _unroot.clear();
    _unroot.push_back(outgroup);

    debug_string("setting the unroots children's parents to the new node");
    ur->_lchild->_parent = ur;
    ur->_rchild->_parent = ur;
    ur->_parent->_parent = ur;
    ur->_children = true;
    debug_print("ur->_parent->_parent: %p, ur->_lchild->_parent: %p, ur->_rchild->_parent: %p",
            ur->_parent->_parent, ur->_lchild->_parent, ur->_rchild->_parent);

    node_t* p = outgroup->_parent;
    outgroup->_parent = nullptr;
    debug_print("p: %p, p->_parent: %p", p, p->_parent);
    if(outgroup != p->_lchild){
        debug_string("swapping the new outgroups parent's children");
        std::swap(p->_lchild, p->_rchild);
    }
    _unroot.push_back(p);
    p->_lchild=nullptr;
    debug_print("p->_parent: %p, p->_lchild: %p, p->_rchild: %p", p->_parent,
            p->_lchild, p->_rchild);
    p->swap_parent(nullptr);
    debug_print("p->_parent: %p, p->_lchild: %p, p->_rchild: %p", p->_parent,
            p->_lchild, p->_rchild);
    make_flat_tree(std::vector<node_t*>(_unroot));
    ur->_lchild = nullptr;
    ur->_rchild = nullptr;
    delete ur;
}

tree_t& tree_t::set_outgroup(const string& outgroup){
    if(_size <= 2) return *this;
    if(is_rooted()){
        debug_string("tree is rooted, unrooting it");
        make_unrooted();
    }
    assert_string(_unroot.size() == 3, "not an unrooted tree");
    node_t* o = nullptr;
    for(size_t i = 0;i<_size;++i){
        if(_tree[i]._label == outgroup) o = _tree+i;
    }
    assert_string(o!=nullptr, "could not find outgroup label");
    set_root(o);
    return *this;
}

/*
 * A function to reorient the tree, because we moved the root. When we move the
 * root, the direction of the parents is wrong. Specifically, for an interior
 * node that WASN'T the unroot, two children will be pointing at each other. So
 * to fix, we need to swap the mismatched child with the parent to make the
 * direections add up. So, call this funciton with the address of the new
 * parent. We swap and recurse. Eventually, the orientation is correct, and we
 * can stop.
 */
void node_t::swap_parent(node_t* p){
    debug_print("in %p, label: %s, _lchild: %p, _rchild: %p", this,
            _label.c_str(), _lchild, _rchild);
    if(p == _lchild){
        debug_print("swapping _lchild: %p and _parent: %p", _lchild, _parent);
        std::swap(_parent,_lchild);
        _lchild->swap_parent(this);
    }
    else if(p == _rchild){
        debug_print("swapping _rchild: %p and _parent: %p", _lchild, _parent);
        std::swap(_parent,_rchild);
        _rchild->swap_parent(this);
    }
}

node_t* node_factory(node_t* lchild, node_t* rchild){
    node_t* ret = new node_t;
    debug_print("lchild to_string: %s, rchild to_string: %s",
            lchild->to_string().c_str(), rchild->to_string().c_str());
    ret->_lchild = lchild;
    ret->_rchild = rchild;
    assert_string(ret->_lchild && ret->_rchild, "both children are not null");
    ret->_children = true;
    lchild->_parent = ret;
    rchild->_parent = ret;
    return ret;
}

tree_t::tree_t(const tree_t& t){
    _tree = nullptr;
    make_flat_tree(t._unroot);
}

//Traverses the tree, and compresses it into an array
void tree_t::make_flat_tree(const vector<node_t*>& unroot){
    unordered_map<node_t*, node_t*> node_map;
    stack<node_t*> node_stack;
    queue<node_t*> node_q; //too many ueue to type each time

    for(size_t i=0;i<unroot.size(); ++i){
        node_stack.push(unroot[i]);
        node_q.push(unroot[i]);
    }
    debug_print("node_stack.size(): %lu", node_stack.size());

    while(!node_stack.empty()){
        debug_print("node stack top: %p", node_stack.top());
        node_t* cur = node_stack.top(); node_stack.pop();
        debug_string(cur->to_string().c_str());
        if(cur->_lchild && cur->_rchild){
            debug_print("pushing _lchild: %p and _rchild:%p", cur->_lchild,
                    cur->_rchild);
            node_stack.push(cur->_lchild);
            node_stack.push(cur->_rchild);
            node_q.push(cur->_lchild);
            node_q.push(cur->_rchild);
        }
    }

    _size = node_q.size();
    auto old_tree = _tree;
    _tree = new node_t[_size];
    debug_print("_tree pointer: %p, last element: %p", _tree, _tree+_size);
    size_t cur_index = 0;

    while(!node_q.empty()){
        debug_print("current_index: %lu, _size: %lu", cur_index, _size);
        node_t* cur = node_q.front(); node_q.pop();
        _tree[cur_index] = node_t(*cur);
        debug_string(_tree[cur_index]._label.c_str());
        node_map[cur] = _tree+cur_index;
        cur_index++;
    }

    _unroot.clear();

    for(auto &i:unroot){
        debug_string("updating children")
            _unroot.push_back(node_map.at(i));
        _unroot.back()->update_children(node_map);
        debug_string(_unroot.back()->to_string().c_str());
    }

    if(old_tree != nullptr){
        delete[] old_tree;
    }
    debug_print("new tree to_string(): %s", to_string().c_str());
}

tree_t::tree_t(const vector<node_t*>& unroot){
    _tree = nullptr;
    debug_string(to_string().c_str());
    make_flat_tree(unroot);
}

tree_t::tree_t(const string& newick){
    _unroot = make_tree_from_newick(newick, _size);
    _tree = nullptr;
    _tree = _unroot.front();
    make_flat_tree(std::vector<node_t*>(_unroot));
}

tree_t::~tree_t(){
    debug_print("_tree: %p", _tree);
    if(_tree)
        delete[] _tree;
}

tree_t& tree_t::operator=(tree_t t){
    std::swap(_tree, t._tree);
    std::swap(_unroot, t._unroot);
    std::swap(_size, t._size);
    return *this;
}

std::vector<double> tree_t::calc_distance_matrix(){
    auto lm = make_label_map();
    debug_print_map("label map", lm);
    auto f = calc_distance_matrix(lm);
    size_t size = lm.size();
    std::vector<double> r(f, f+size*size); //copy from f to f+size*size
    delete[] f;
    debug_string(to_string().c_str());
    debug_matrix("r", r, size);
    return r;
}

//we use a label to index map to make the matrix well ordered
//this is so we can do a blind average later on, and not have to worry about
//the ordering of the array
double* tree_t::calc_distance_matrix(const std::unordered_map<string, size_t>& label_map){
    debug_string("calc_distance_matrix with label map");
    size_t row_size = label_map.size();
    double* dists = new double[row_size*row_size];
    calc_distance_matrix(label_map, dists);
    return dists;
}

//I can do this better, and make the node_t smaller.
//Take to do this, perform a recursive algorithm.
//  Start at the root and find distances to all the leaves. This involves basically finding all the leaves
//      Since the tree is ultrametric, we only need to find the distance once, and then set every pair to 2 that distance
//  Then recursively call the same routine on the two children of the tree.
//  Because of the ordering, the distance matrix will update with the smallest size found.
//  This can even work on trees with an unroot, as long as there is a special case for the unroot
//The main advantage of this method is to remove the need for a parent pointer in the node_t
//This saves on quite a bit of space, and allows for larger trees to fit in cache
void tree_t::calc_distance_matrix(const std::unordered_map<string, size_t>& label_map, double* dists){
    size_t row_size = label_map.size();
    for(size_t i=0;i<_size;++i){
        //if the node has no children, then it is a leaf, and we need to find distances
        if(!_tree[i]._children){
            size_t matrix_index = label_map.at(_tree[i]._label);
            dists[matrix_index*row_size] = 0;
            for(size_t j=0;j<_size;++j){
                if(!_tree[j]._children){
                    size_t dest_matrix_index = label_map.at(_tree[j]._label);
                    debug_print("calculating distance for (%lu,%lu), putting in: (%lu,%lu)",
                            i, j, matrix_index,dest_matrix_index);
                    dists[row_size*matrix_index+dest_matrix_index] = calc_distance(_tree+i,_tree+j);
                }
            }
        }
    }
}


//need to make a map of labels to indices, but the order doesnt really matter
//so, this is inteded to be called for on the first tree, and never again
//the return of this function is meant to be fed into the function
//  calc_distance_matrix();
//  debug_print("_tree pointer: %p", _tree);
//so that it can calculate ta specific matrix that is ''well ordered''
std::unordered_map<string, size_t> tree_t::make_label_map(){
    size_t label_index = 0;
    std::unordered_map<string, size_t> label_map;
    for(size_t i = 0; i<_size; ++i){
        if(!_tree[i]._children){
            label_map[_tree[i]._label] = label_index++;
        }
    }
    return label_map;
}

//calculate the distance between two nodes
//game plan:
//  make a list of parents for each node
//  compare those lists from the back (ie, root first)
//  when those lists diverge, thats the common parent
double tree_t::calc_distance(node_t* src, node_t* dst){
    debug_print("calculating distance between (%p, %p)", src, dst);
    if(src==dst){
        debug_string("src and dst are the same, returning zero");
        return 0.0;
    }
    auto src_list = get_parents_of(src);
    auto dst_list = get_parents_of(dst);

    debug_print("src list size: %lu, dst list size: %lu", src_list.size(),
            dst_list.size());
    size_t src_index = src_list.size()-1;
    size_t dst_index = dst_list.size()-1;


    //walk through the list until the lists diverge
    debug_string("starting to walk the parent lists");
    while(true){
        assert_string((src_index < src_list.size() || dst_index < dst_list.size())
                , "Parent lists don't converge, but not same index");

        if(src_list[src_index] != dst_list[dst_index]){
            src_index++; dst_index++;//need to back up a step
            break;
        }
        src_index--; dst_index--;
    }

    //calculating common parent can be faster, since I have a list of parents already, but this is fine
    double ret =  parent_distance(src, src_list[src_index]) + parent_distance(dst,dst_list[dst_index]);
    debug_print("returning the distance %f", ret);
    return ret;
}

vector<node_t*> tree_t::get_parents_of(node_t* cur_node){
    debug_print("getting the parents of %s", cur_node->_label.c_str());
    vector<node_t*> parent_list;
    parent_list.reserve(_size);
    parent_list.push_back(cur_node);

    while(cur_node->_parent && cur_node->_parent != cur_node){
        debug_print("current node: %p", cur_node);
        parent_list.push_back(cur_node->_parent);
        debug_print("setting current node to parent: %p", cur_node->_parent);
        cur_node = cur_node->_parent;
    }
    if(cur_node->_parent == nullptr){
        parent_list.push_back(nullptr);
    }
    return parent_list;
}

double tree_t::parent_distance(node_t* child, node_t* parent){
    double distance = 0;
    while(child!=parent){
        distance+=child->_weight;
        child = child->_parent;
    }
    return distance;
}

string node_t::to_string(int p){
    ostringstream ret;
    if(_lchild && _rchild){
        ret<<"("<<_lchild->to_string(p)
            <<","<<_rchild->to_string(p)<<")";
    }
    else{
        ret<<_label;
    }
    if(_weight!=0.0){
        ret<<":"<<std::fixed<<std::setprecision(p)<<_weight;
    }
    return ret.str();
}

string tree_t::to_string(int p) const{
    ostringstream ret;

    if(_unroot.size()>1)
        ret<<"(";
    for(size_t i=0;i<_unroot.size();++i){
        ret<<_unroot[i]->to_string(p);
        if(i!=_unroot.size()-1)
            ret<<",";
    }
    if(_unroot.size()>1)
        ret<<")";

    if(ret.str() != "")
        ret<<";";

    return ret.str();
}

string tree_t::print_labels() const{
    ostringstream ret;
    for(size_t i=0;i<_size;++i){
        ret<<_tree[i]._label<< "(" << _tree[i]._parent <<"," <<_tree[i]._lchild
            <<","<<_tree[i]._rchild<<")";
        if(i!=_size-1) ret<<" | ";
    }
    return ret.str();
}

void tree_t::set_weights(function<double(size_t)> w_func, double max){
    size_t depth = get_depth();
    debug_print("max depth: %lu", depth);
    if(max==0.0){
        for(size_t i=0;i<depth;++i){
            max+=w_func(i);
            debug_print("w_func(%lu)=%f", i, w_func(i));
        }
    }
    debug_print("max: %f", max);
    if(_unroot.size() == 1){
        _unroot.front()->set_weights_as_root(w_func, 0, max);
    }
    else{
        for(auto& n : _unroot){
            n->set_weights(w_func, 0, max);
        }
    }
}

void tree_t::set_weights(const vector<double>& w_vec, double max){
    set_weights([&w_vec](size_t d){
            assert_string(d < w_vec.size(), "out of bounds for passed double vector");
            return d==0 ? w_vec[d]/2.0 : w_vec[d];
            }, max);
}

void tree_t::set_weights(double w, double max){
    set_weights([w](size_t d) -> double {return d==0 ? w/2.0 : w;}, max);
}

void tree_t::set_weights_constant(double c){
    for(auto &&n:_unroot){
        n->set_weights_constant(c);
    }
}

tree_t& tree_t::clear_weights(){
    set_weights_constant(0.0);
    return *this;
}

tree_t& tree_t::sort(){
    assert_string(_unroot.size() <= 3, "the unroot is has a size different than expected");
    vector<string> label_vector;
    label_vector.reserve(3);
    for(auto && n : _unroot){
        label_vector.push_back(n->sort());
    }
    for(size_t i=0;i<_unroot.size();++i){
        for(size_t k=i;k<_unroot.size();++k){
            if(i==k){ continue;}
            if(label_vector[i] > label_vector[k]){
                std::swap(label_vector[i], label_vector[k]);
                std::swap(_unroot[i], _unroot[k]);
            }
        }
    }
    return *this;
}

size_t tree_t::get_depth() const{
    size_t max=0;
    for(auto& n:_unroot){
        size_t tmp = n->calc_max_depth();
        if(max<tmp) max = tmp;
    }
    if(_unroot.size() == 1) max-=1;
    return max;
}

bool tree_t::is_rooted(){
    //_unroot.size() is a size_t, which is unsigned,
    return _unroot.size()<=2;
}

void tree_t::make_unrooted(){
    assert_string(is_rooted(),"trying to unroot a tree, its already unrooted");
    assert_string(_size>2,"tree too small to unroot");

    while(_unroot.size()!=3){
        node_t* tmp_n = nullptr;
        int idx = 0;
        for(size_t i = 0; i < _unroot.size(); ++i){
            if(_unroot[i]->_children){
                tmp_n = _unroot[i];
                idx = i;
                break;
            }
        }
        debug_print("tmp_n: %p", tmp_n);
        assert_string(tmp_n != nullptr, "could not find node to reroot");
        _unroot.erase(_unroot.begin()+idx);
        debug_print("pushing _lchild: %p and _rchild: %p", tmp_n->_lchild,
                tmp_n->_rchild);
        debug_print("before pushing children _unroot.size(): %lu", _unroot.size());
        _unroot.push_back(tmp_n->_lchild);
        _unroot.push_back(tmp_n->_rchild);
        debug_print("after pushing children, _unroot.size(): %lu", _unroot.size());
        tmp_n->_lchild->_parent=nullptr;
        tmp_n->_rchild->_parent=nullptr;
        tmp_n->_lchild = nullptr;
        tmp_n->_rchild = nullptr;
        tmp_n->_children = false;
        debug_print("tmp_n: %p", tmp_n);
    }
    make_flat_tree(vector<node_t*>(_unroot));
    debug_print("unroot size after making flat: %lu", _unroot.size());
}

std::ostream& operator<<(std::ostream& os, const tree_t& t){
    return os<<t.to_string();
}
