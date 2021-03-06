﻿#pragma once

#include "deque_iterator.h"
#include "allocator.h"
#include "uninitialized.h"
#include "stl_algobase.h"

namespace MiniSTL {

template<class T, class Alloc = simpleAlloc<T>, size_t BufSiz = 0>
class deque {
public:// alias declarations
	using value_type = T;
	using pointer = T* ;
	using reference = T& ;
	using const_reference = const T&;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using iterator = typename __deque_iterator<T,T&,T*, BufSiz>::iterator;
	using reverse_iterator = MiniSTL::reverse_iterator<iterator>;
	using const_iterator = typename __deque_iterator<T, T&, T*, BufSiz>::const_iterator;
	using const_reverse_iterator = MiniSTL::reverse_iterator<const_iterator>;

private:// Internal alias declarations
	using map_pointer = pointer* ;
	using data_allocator = simpleAlloc<value_type>;
	using map_allocator = simpleAlloc<pointer>;

private:// data member
	iterator start;// 第一个节点
	iterator finish;// 最后一个节点
	map_pointer map;// 指向节点的指针
	size_type map_size;

private:// Internal member function
	size_type initial_map_size() { return 8U; }
	size_type buffer_size() const { return iterator::buffer_size(); }
	value_type* allocate_node() {return data_allocator::allocate(__deque_buf_size(sizeof(value_type)));}
	void deallocate_node(value_type* p) { data_allocator::deallocate(p, __deque_buf_size(sizeof(value_type)));}
	void create_map_and_nodes(size_type);
	void fill_initialized(size_type, const value_type&value=value_type());
	void reallocate_map(size_type, bool);
	void reverse_map_at_back(size_type nodes_to_add = 1);
	void reverse_map_at_front(size_type nodes_to_add = 1);
	void push_back_aux(const value_type&);
	void push_front_aux(const value_type&);
	void pop_back_aux();
	void pop_front_aux();
	iterator insert_aux(iterator, const value_type&);

public:// ctor && dtor
	deque():start(), finish(), map(nullptr), map_size(0) { fill_initialized(0, value_type()); }
	explicit deque(size_type n, const value_type& value=value_type()) :start(), finish(), map(nullptr), map_size(0) { fill_initialized(n, value); }
	// without this,InputIterator can be deduced as int
	deque(int n,int value):start(), finish(), map(nullptr), map_size(0) { fill_initialized(n, value); }
	template<class InputIterator>
	deque(InputIterator first, InputIterator last);
	~deque();

public:// getter
	const_iterator cbegin() const noexcept { return start; }
	const_iterator cend() const noexcept { return finish; }
	const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(finish); }
	const_reverse_iterator crend() const noexcept { return const_reverse_iterator(start); }
	const_reference operator[](size_type n) const { return start[static_cast<difference_type>(n)]; }
	size_type size() const noexcept { return finish - start; }
	bool empty() const noexcept { return finish == start; }

public:// setter
	iterator begin() { return start; }
	iterator end() { return finish; }
	reverse_iterator rbegin() { return reverse_iterator(finish); }
	reverse_iterator rend() { return reverse_iterator(start); }
	reference operator[](size_type n) {return start[static_cast<difference_type>(n)];}
	reference front() { return *start; }
	reference back() { return *(finish - 1); }

public:// interface about insert and erase
	void push_back(const value_type&);
	void push_front(const value_type&);
	void pop_back();
	void pop_front();
	void clear();
	iterator erase(iterator pos);
	iterator erase(iterator first, iterator last);
	iterator insert(iterator pos, const value_type &value);
};

template<class T, class Alloc, size_t BufSiz>
void deque<T, Alloc, BufSiz>::create_map_and_nodes(size_type n) {
	//所需节点数（整除则多配置一个）
	size_type num_nodes = n / iterator::buffer_size() + 1;
	//一个map至少管理8个节点，至多管理num_nodes+2个
	map_size = max(initial_map_size(),num_nodes+2);
	map = map_allocator::allocate(map_size);
	//令nstart与nfinish指向map所拥有的全部node的中部,以便日后扩充头尾
	map_pointer nstart = map + (map_size - num_nodes) / 2;
	map_pointer nfinish = nstart + num_nodes - 1;

	map_pointer cur;
	try {
		//为每一个节点配置空间
		for (cur = nstart; cur <= nfinish; ++cur)
			*cur = allocate_node();
	}
	catch (std::exception&) {
		clear();
	}
	start.set_node(nstart);
	finish.set_node(nfinish);
	start.cur = start.first;
	finish.cur = finish.first + n % buffer_size();//若n%buffer_size==0,会多配置一个节点，此时cur指向该节点头部
}

template<class T, class Alloc, size_t BufSiz>
void deque<T, Alloc, BufSiz>::fill_initialized(size_type n, const value_type & value) {
	create_map_and_nodes(n);
	map_pointer cur;
	try {
		//为每个缓冲区设定初值
		for (cur = start.node; cur < finish.node; ++cur)
			uninitialized_fill(*cur, *cur + iterator::buffer_size(), value);
		//最后一个缓冲区只设定至需要处
		uninitialized_fill(finish.first, finish.cur, value);
	}
	catch (std::exception&) {
		clear();
	}
}

template<class T, class Alloc, size_t BufSiz>
inline void deque<T, Alloc, BufSiz>::reallocate_map(size_type nodes_to_add, bool add_at_front) {
	size_type old_num_nodes = finish.node - start.node + 1;
	size_type new_num_nodes = old_num_nodes + nodes_to_add;

	map_pointer new_nstart;
	if (map_size > 2 * new_num_nodes) {
		//某一端出现失衡，因此释放存储区完成重新中央分配，
		//规定新的nstart，若添加在前端则向后多移动n个单位
		new_nstart = map + (map_size - new_num_nodes) / 2 + (add_at_front ? nodes_to_add : 0);
		if (new_nstart < start.node)//整体前移
			copy(start.node, finish.node + 1, new_nstart);
		else//整体后移
			std::copy_backward(start.node, finish.node + 1, new_nstart + old_num_nodes);
	}
	else {
		size_type new_map_size = map_size + max(map_size, nodes_to_add) + 2;
		//分配新空间
		map_pointer new_map = map_allocator::allocate(new_map_size);
		new_nstart = new_map + (new_map_size - new_num_nodes) / 2 + (add_at_front ? nodes_to_add : 0);
		//拷贝原有内容
		copy(start.node, finish.node + 1, new_nstart);
		//释放原map
		map_allocator::deallocate(map, map_size);
		//重新设定map
		map = new_map;
		map_size = new_map_size;
	}

	//设定start与finish
	start.set_node(new_nstart);
	finish.set_node(new_nstart + old_num_nodes - 1);//注意并非new_num,接下来的设定转交其他函数处理
}

template<class T, class Alloc, size_t BufSiz>
inline void deque<T, Alloc, BufSiz>::reverse_map_at_back(size_type nodes_to_add) {
	//map_size-(finish.node-map+1)== 后端剩余node个数
	if (nodes_to_add > map_size - (finish.node - map + 1))
		reallocate_map(nodes_to_add, false);
}

template<class T, class Alloc, size_t BufSiz>
inline void deque<T, Alloc, BufSiz>::reverse_map_at_front(size_type nodes_to_add) {
	//start.node-map==前端剩余node个数
	if (nodes_to_add > start.node - map)
		reallocate_map(nodes_to_add, true);
}

template<class T, class Alloc, size_t BufSiz>
inline void deque<T, Alloc, BufSiz>::push_back_aux(const value_type & value) {
	value_type value_copy = value;
	reverse_map_at_back();//若符合某条件则重新更换map
	*(finish.node + 1) = allocate_node();//配置新节点
	try {
		construct(finish.cur, value_copy);
		finish.set_node(finish.node + 1);
		finish.cur = finish.first;//更新finish.cur为当前first
	}
	catch(std::exception&){
		deallocate_node(*(finish.node + 1));
	}
}

template<class T, class Alloc, size_t BufSiz>
inline void deque<T, Alloc, BufSiz>::push_front_aux(const value_type& value) {
	value_type value_copy = value;
	reverse_map_at_front();//若符合某条件则重新更换map
	*(start.node - 1) = allocate_node();//配置新节点
	try {
		start.set_node(start.node - 1);
		start.cur = start.last - 1;
		construct(start.cur, value);
	}
	catch (std::exception&){
		start.set_node(start.node + 1);
		start.cur = start.first;
		deallocate_node(*(start.node - 1));
	}
}

template<class T, class Alloc, size_t BufSiz>
inline void deque<T, Alloc, BufSiz>::pop_back_aux() {
	data_allocator::deallocate(finish.first);
	finish.set_node(finish.node - 1);
	finish.cur = finish.last - 1;
	destroy(finish.cur);
}

template<class T, class Alloc, size_t BufSiz>
inline void deque<T, Alloc, BufSiz>::pop_front_aux() {
	destroy(start.cur);
	data_allocator::deallocate(start.first);
	start.set_node(finish.node + 1);
	start.cur = start.first;
}

template<class T, class Alloc, size_t BufSiz>
typename deque<T, Alloc, BufSiz>::iterator 
deque<T, Alloc, BufSiz>::insert_aux(iterator pos, const value_type & value) {
	difference_type index = pos - start;//插入点之前的元素个数
	value_type value_copy = value;
	if (index < size() / 2) {//前移
		//插图见书
		push_front(front());//最前端加入哨兵以作标识，注意此时start发生了改变
		iterator front1 = start + 1;//待复制点
		iterator front2 = front1 + 1;//复制起始点
		pos1 = start + index + 1;//复制终止点（可否以pos直接代替pos1？）
		copy(front2, pos1, front1);//移动元素
	}
	else {
		//过程类似于上
		push_back(back());
		iterator back1 = finish - 1;;
		iterator back2 = back1 - 1;
		pos = start + index;
		std::copy_backward(pos, back2, back1);
	}
	*pos = value_copy;
	return pos;
}

template<class T, class Alloc, size_t BufSiz>
inline deque<T, Alloc, BufSiz>::~deque(){
	destroy(start, finish);
	for (auto temp = start.node; temp != finish.node; ++temp) deallocate_node(*temp);
	deallocate_node(*finish.node);
	map_allocator::deallocate(map,map_size);
}

template<class T, class Alloc, size_t BufSiz>
inline void deque<T, Alloc, BufSiz>::push_back(const value_type& value) {
	//finish的cur指向最后一个元素的下一个位置，因此if语句表征至少还有一个备用空间
	if (finish.cur != finish.last - 1) {
		construct(finish.cur, value);
		++finish.cur;
	}
	else
		//最终缓冲区已无或仅剩一个空间（我认为必然为仅剩一个空间的状态）
		push_back_aux(value);
}

template<class T, class Alloc, size_t BufSiz>
inline void deque<T, Alloc, BufSiz>::push_front(const value_type& value) {
	if (start.cur != start.first) {
		construct(start.cur - 1, value);
		--start.cur;
	}
	else
		push_front_aux(value);
}

template<class T, class Alloc, size_t BufSiz>
inline void deque<T, Alloc, BufSiz>::pop_back() {
	if (finish.cur != finish.first) {
		//缓冲区至少存在一个元素
		--finish.cur;
		destroy(finish.cur);
	}
	else
		pop_back_aux();
}

template<class T, class Alloc, size_t BufSiz>
inline void deque<T, Alloc, BufSiz>::pop_front() {
	if (start.cur != start.last - 1) {
		destroy(start.cur);
		++start.cur;
	}
	else
		pop_front_aux();
}

template<class T, class Alloc, size_t BufSiz>
inline void deque<T, Alloc, BufSiz>::clear() {
	//清空所有node，保留唯一缓冲区（需要注意的是尽管map可能存有更多节点，但有[start,finish]占据内存
	for (map_pointer node = start.node + 1; node < finish.node; ++node) {//内部均存有元素
		destroy(*node, *node + buffer_size());//析构所有元素
		data_allocator::deallocate(*node, buffer_size());
	}
	if (start.node != finish.node) {//存在头尾两个缓冲区
		//析构其中所有元素
		destroy(start.cur, start.last);
		destroy(finish.first, finish.cur);
		//保存头部，释放尾部
		data_allocator::deallocate(finish.first, buffer_size());
	}
	else
		destroy(start.cur, finish.cur);//利用finish.cur标记末尾
	finish = start;
}

template<class T, class Alloc, size_t BufSiz>
typename deque<T, Alloc, BufSiz>::iterator 
deque<T, Alloc, BufSiz>::erase(iterator pos) {
	iterator next = pos + 1;
	difference_type index = pos - start;//清除点前的元素个数
	if (index < size() / 2) {//后移开销较低
		std::copy_backward(start, pos, pos);
		pop_front();
	}
	else {
		copy(next, finish, pos);
		pop_back();
	}
	return start + index;
}

template<class T, class Alloc, size_t BufSiz>
typename deque<T, Alloc, BufSiz>::iterator 
deque<T, Alloc, BufSiz>::erase(iterator first, iterator last) {
	if (first == start && last == end) {
		clear();
		return finish;
	}
	else {
		difference_type n = last - first; //清除区间长度
		difference_type elems_before = first - start; //前方元素个数
		if (elems_before < (size() - n) / 2) {//后移开销较低
			std::copy_backward(start, first, last);
			iterator new_start = start + n;//标记新起点
			destroy(start, new_start);//析构多余元素
			//释放多余缓冲区
			for (map_pointer cur = start.node; cur < new_start.node; ++cur)
				data_allocator::deallocate(*cur, buffer_size());
			start = new_start;
		}
		else {//前移开销较低		
			copy(last, finish, first);
			iterator new_finish = finish - n;//标记末尾
			destroy(new_finish, finish);
			//释放多余缓冲区
			for (map_pointer cur = new_finish.node + 1; cur <= finish.node; ++cur)
				data_allocator::deallocate(*cur, buffer_size());
			finish = new_finish;
		}
		return start + elems_before;
	}
}

template<class T, class Alloc, size_t BufSiz>
typename deque<T, Alloc, BufSiz>::iterator 
deque<T, Alloc, BufSiz>::insert(iterator pos, const value_type& value) {
	if (pos.cur == start.cur) {
		push_front(value);
		return start;
	}
	else if (pos.cur == finish.cur) {
		push_back(value);
		iterator temp = finish - 1;
		return temp;
	}
	else
		returninsert_aux(pos, value);
}

template<class T, class Alloc, size_t BufSiz>
template<class InputIterator>
inline deque<T, Alloc, BufSiz>::deque(InputIterator first, InputIterator last){
	size_type size = MiniSTL::distance(first, last);
	InputIterator mid = first;
	advance(mid, size);
	copy(first, mid, begin());
	insert(end(), mid,last);
}

}// end namespace::std
