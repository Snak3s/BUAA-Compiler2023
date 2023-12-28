#ifndef __CPL_EXPRHASH_H__
#define __CPL_EXPRHASH_H__

#include <vector>

#include "types.h"


namespace ExprHash {

using namespace Types;


static const Type TItem  = Type();
static const Type TSet   = Type();
static const Type TArray = Type();
static const Type TConst = Type();
static const Type TReg   = Type();

class HashItem {
public:
	int hashValue = 0;

	virtual Type itemType() const { return TItem; }

	virtual void calcHashValue() {}

	virtual bool equals(HashItem *item) = 0;
};

class HashSet final : public HashItem {
public:
	static const int mod = 998244353;
	static vector <HashSet *> allocated;

	vector <HashItem *> items;

	HashSet() {
		allocated.emplace_back(this);
	}

	int f(int v) {
		int r = (1ll * v * v % mod * v + v) % mod;
		r ^= (1ll * v * v % mod) >> 7;
		r ^= (v & 0x3fff) << 17;
		return r;
	}

	Type itemType() const { return TSet; }

	static bool compare(HashItem *a, HashItem *b) {
		return a->hashValue < b->hashValue;
	}

	void calcHashValue() {
		sort(items.begin(), items.end(), compare);
		hashValue = 1;
		for (HashItem *item : items) {
			hashValue = 1ll * hashValue * f(item->hashValue) % mod;
		}
	}

	bool equals(HashItem *item) {
		if (item->itemType() != TSet) {
			return false;
		}
		HashSet *hSet = (HashSet *)item;
		if (items.size() != hSet->items.size()) {
			return false;
		}
		for (int i = 0; i < items.size(); i++) {
			if (!items[i]->equals(hSet->items[i])) {
				return false;
			}
		}
		return true;
	}

	static void clear() {
		for (HashSet *hSet : allocated) {
			delete hSet;
		}
		allocated.clear();
	}
};

class HashArray final : public HashItem {
public:
	static const int mod = 998244353;
	static vector <HashArray *> allocated;

	vector <HashItem *> items;

	HashArray() {
		allocated.emplace_back(this);
	}

	int f(int v) {
		int r = (1ll * v * v % mod * v + v) % mod;
		r ^= (1ll * v * v % mod) >> 7;
		r ^= (v & 0x3fff) << 17;
		return r;
	}

	Type itemType() const { return TArray; }

	void calcHashValue() {
		hashValue = 0;
		int count = 1;
		for (HashItem *item : items) {
			hashValue = (hashValue + 1ll * count * f(item->hashValue)) % mod;
			count++;
		}
	}

	bool equals(HashItem *item) {
		if (item->itemType() != TArray) {
			return false;
		}
		HashArray *hArray = (HashArray *)item;
		if (items.size() != hArray->items.size()) {
			return false;
		}
		for (int i = 0; i < items.size(); i++) {
			if (!items[i]->equals(hArray->items[i])) {
				return false;
			}
		}
		return true;
	}

	static void clear() {
		for (HashArray *hArray : allocated) {
			delete hArray;
		}
		allocated.clear();
	}
};

class HashConst final : public HashItem {
public:
	static const int mod = 998244353;
	static vector <HashConst *> allocated;

	int value;

	HashConst(int value) {
		this->value = value;
		calcHashValue();
		allocated.emplace_back(this);
	}

	Type itemType() const { return TConst; }

	void calcHashValue() {
		hashValue = (value % mod + mod) % mod;
	}

	bool equals(HashItem *item) {
		if (item->itemType() != TConst) {
			return false;
		}
		HashConst *hConst = (HashConst *)item;
		return value == hConst->value;
	}

	static void clear() {
		for (HashConst *hConst : allocated) {
			delete hConst;
		}
		allocated.clear();
	}
};

class HashReg final : public HashItem {
public:
	static const int mod = 998244353;
	static vector <HashReg *> allocated;

	int reg;

	HashReg(int reg) {
		this->reg = reg;
		calcHashValue();
		allocated.emplace_back(this);
	}

	Type itemType() const { return TReg; }

	void calcHashValue() {
		hashValue = reg;
		hashValue = (hashValue % mod + mod) % mod;
	}

	bool equals(HashItem *item) {
		if (item->itemType() != TReg) {
			return false;
		}
		HashReg *hReg = (HashReg *)item;
		return reg == hReg->reg;
	}

	static void clear() {
		for (HashReg *hReg : allocated) {
			delete hReg;
		}
		allocated.clear();
	}
};

vector <HashSet *> HashSet::allocated;
vector <HashArray *> HashArray::allocated;
vector <HashConst *> HashConst::allocated;
vector <HashReg *> HashReg::allocated;

}

#endif