#include "newick.h"
#include "debug.h"

#include <stack>
using std::stack;
#include <cctype>
using std::isdigit;
#include <string>
using std::string;
#include <memory>
using std::shared_ptr;

struct ptr_node_t{
    ptr_node_t* _lchild;
    ptr_node_t* _rchild;
    string _label;
    float _weight;
    ptr_node_t(): _lchild(0), _rchild(0) {}
    size_t count_leaves() const{
        if(!_lchild && !_rchild) return 1;
        return _lchild->count_leaves() + _rchild->count_leaves()+1;
    }
    ~ptr_node_t(){
        if(_lchild && _rchild){
            delete _lchild;
            delete _rchild;
        }
    }
};

inline size_t skip_whitespace(const string& s, size_t index){
    while(isspace(s[index])!=0 && index < s.size()) index++;
    return index;
}

void copy_to_node(ptr_node_t* t, node_t* n){
    n->_weight = t->_weight;
}

void set_childen(node_t* n, size_t i, size_t j){
    n->_children = true;
    n->_lchild = i;
    n->_rchild = j;
}

node_t* convert_to_packed_tree(ptr_node_t* root, size_t& s){
    size_t size = root->count_leaves();
    s = size;
    size_t current_index=0;
    node_t* tree = new node_t[size];
    copy_to_node(root, tree+(current_index++));

    stack<ptr_node_t*> ptr_node_stack;
    ptr_node_stack.push(root);
    stack<node_t*> node_stack;
    node_stack.push(tree+0);
    while(!node_stack.empty()){
        ptr_node_t* current_ptr_node = ptr_node_stack.top(); ptr_node_stack.pop();
        node_t* current_node = node_stack.top(); node_stack.pop();
        if(current_ptr_node->_lchild&&current_ptr_node->_rchild){
            size_t lchild_index = current_index;
            size_t rchild_index = current_index+1;
            current_index+=2;
            copy_to_node(current_ptr_node->_lchild, tree+lchild_index);
            copy_to_node(current_ptr_node->_rchild, tree+rchild_index);
            set_childen(current_node, lchild_index, rchild_index);
            ptr_node_stack.push(current_ptr_node->_lchild);
            ptr_node_stack.push(current_ptr_node->_rchild);
            node_stack.push(tree+lchild_index);
            node_stack.push(tree+rchild_index);
        }
        else{
            current_node->_children=false;
        }
    }
    return tree;
}

node_t* make_tree_from_newick(const string& s, size_t& tree_size){
    stack<ptr_node_t*> node_stack;
    node_stack.push(new ptr_node_t);
    size_t index=0;

    while(index < s.size()){
        index = skip_whitespace(s, index);
        if(s[index] == '(' || s[index]==','){
            ptr_node_t* tmp = new ptr_node_t;
            index++;
            node_stack.push(tmp);
        }
        else if(s[index]==')'){
            ptr_node_t* tmp1 = node_stack.top(); node_stack.pop();
            ptr_node_t* tmp2 = node_stack.top(); node_stack.pop();
            node_stack.top()->_lchild=tmp1;
            node_stack.top()->_rchild=tmp2;
            index++;
        }
        else if(s[index]==';') break;
        else{
            index = skip_whitespace(s, index);
            size_t j=index;
            while(j<s.size() && s[j]!=',' && s[j]!=':') ++j;
            node_stack.top()->_label = s.substr(index, j-index);
            index = j = j+1;
            while(j < s.size() && (isdigit(s[j])!=0 || s[j]=='.')) ++j;
            if(index != j)
                node_stack.top()->_weight = stof( s.substr(index, j-index));
            index = j;
        }
    }
    
    //one final bit of setup to make the whole tree trivalant.
    //We make a special root taxon that gets joined to the top of the stack
    //Its "parent" is the one child it has
    debug_print("stack size at end: %lu", node_stack.size());

    node_stack.top()->_weight = 0;

    return convert_to_packed_tree(node_stack.top(), tree_size);
}