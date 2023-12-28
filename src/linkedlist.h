#ifndef __CPL_LINKED_LIST_H__
#define __CPL_LINKED_LIST_H__


namespace List {

class LinkedListItem {
public:
	LinkedListItem *__ll_next = nullptr;
	LinkedListItem *__ll_prev = nullptr;
};


template <typename T>
class LinkedList {
public:
	LinkedListItem *head = nullptr;
	LinkedListItem *tail = nullptr;

	LinkedList() {
		head = new LinkedListItem();
		tail = new LinkedListItem();
		head->__ll_next = tail;
		tail->__ll_prev = head;
	}

	class iterator {
	public:
		LinkedListItem *ptr = nullptr;

		iterator(LinkedListItem *ptr) {
			this->ptr = ptr;
		}

		iterator &operator ++ () {
			if (ptr != nullptr) {
				ptr = ptr->__ll_next;
			}
			return *this;
		}

		iterator &operator -- () {
			if (ptr != nullptr) {
				ptr = ptr->__ll_prev;
			}
			return *this;
		}

		iterator &operator + (int n) {
			while (n > 0) {
				ptr = ptr->__ll_next;
				n--;
			}
			return *this;
		}

		iterator &operator - (int n) {
			while (n > 0) {
				ptr = ptr->__ll_prev;
				n--;
			}
			return *this;
		}

		T *operator * () {
			return (T *)ptr;
		}

		bool operator == (const iterator &it) {
			return ptr == it.ptr;
		}

		bool operator != (const iterator &it) {
			return ptr != it.ptr;
		}
	};

	T *first() const {
		if (head->__ll_next == tail) {
			return nullptr;
		}
		return (T *)head->__ll_next;
	}

	T *last() const {
		if (tail->__ll_prev == head) {
			return nullptr;
		}
		return (T *)tail->__ll_prev;
	}

	bool empty() const {
		return head->__ll_next == tail;
	}

	void append(T *t) {
		tail->__ll_prev->__ll_next = t;
		t->__ll_prev = tail->__ll_prev;
		tail->__ll_prev = t;
		t->__ll_next = tail;
	}

	void prepend(T *t) {
		head->__ll_next->__ll_prev = t;
		t->__ll_next = head->__ll_next;
		head->__ll_next = t;
		t->__ll_prev = head;
	}

	void insert(T *p, T *t) {
		t->__ll_prev = p->__ll_prev;
		t->__ll_prev->__ll_next = t;
		p->__ll_prev = t;
		t->__ll_next = p;
	}

	void insert(const iterator &it, T *t) {
		LinkedListItem *p = it.ptr;
		t->__ll_prev = p->__ll_prev;
		t->__ll_prev->__ll_next = t;
		p->__ll_prev = t;
		t->__ll_next = p;
	}

	void insertAfter(T *p, T *t) {
		t->__ll_next = p->__ll_next;
		t->__ll_next->__ll_prev = t;
		p->__ll_next = t;
		t->__ll_prev = p;
	}

	void insertAfter(const iterator &it, T *t) {
		LinkedListItem *p = it.ptr;
		t->__ll_next = p->__ll_next;
		t->__ll_next->__ll_prev = t;
		p->__ll_next = t;
		t->__ll_prev = p;
	}

	void insertBefore(T *p, T *t) {
		insert(p, t);
	}

	void insertBefore(const iterator &it, T *t) {
		insert(it, t);
	}

	void erase(T *t) {
		t->__ll_next->__ll_prev = t->__ll_prev;
		t->__ll_prev->__ll_next = t->__ll_next;
	}

	void erase(const iterator &it) {
		LinkedListItem *t = it.ptr;
		t->__ll_next->__ll_prev = t->__ll_prev;
		t->__ll_prev->__ll_next = t->__ll_next;
	}

	void emplace_back(T *t) {
		append(t);
	}

	int size() const {
		int cnt = 0;
		LinkedListItem *item = head->__ll_next;
		while (item != tail) {
			item = item->__ll_next;
			cnt++;
		}
		return cnt;
	}

	T *operator [] (int x) const {
		return *(begin() + x);
	}

	iterator begin() const {
		return iterator(head->__ll_next);
	}

	iterator end() const {
		return iterator(tail);
	}

	iterator rbegin() const {
		return iterator(tail->__ll_prev);
	}

	iterator rend() const {
		return iterator(head);
	}
};

}

#endif