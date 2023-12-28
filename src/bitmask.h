#ifndef __CPL_BITMASK_H__
#define __CPL_BITMASK_H__

#include <vector>


namespace Bits {

using namespace std;


struct Bitmask {
	vector <unsigned long long> bits;
	int len = 0;
	static const int B = 64;

	Bitmask() {}
	Bitmask(int n) {
		resize(n);
	}

	void resize(int n) {
		len = n;
		bits.resize((n + B - 1) / B);
		clear();
	}

	void clear() {
		for (int i = 0; i < bits.size(); i++) {
			bits[i] = 0;
		}
	}

	void fill() {
		for (int i = 0; i < bits.size(); i++) {
			bits[i] = ~(0llu);
		}
	}

	int get(int n) const {
		if (n < 0 || n >= len) {
			return 0;
		}
		return (bits[n / B] >> (n % B)) & 1;
	}

	void set(int n, int v) {
		if (n < 0 || n >= len) {
			return;
		}
		unsigned long long b = (v != 0);
		bits[n / B] &= ~(1llu << (n % B));
		bits[n / B] |= b << (n % B);
	}

	void bitwiseNot() {
		for (int i = 0; i < bits.size(); i++) {
			bits[i] = ~bits[i];
		}
	}

	void bitwiseAnd(const Bitmask &o) {
		for (int i = 0; i < bits.size() && i < o.bits.size(); i++) {
			bits[i] &= o.bits[i];
		}
	}

	void bitwiseOr(const Bitmask &o) {
		for (int i = 0; i < bits.size() && i < o.bits.size(); i++) {
			bits[i] |= o.bits[i];
		}
	}

	void bitwiseXor(const Bitmask &o) {
		for (int i = 0; i < bits.size() && i < o.bits.size(); i++) {
			bits[i] ^= o.bits[i];
		}
	}

	void bitwiseDiff(const Bitmask &o) {
		for (int i = 0; i < bits.size() && i < o.bits.size(); i++) {
			bits[i] &= ~o.bits[i];
		}
	}

	int count() {
		int r = 0;
		for (int i = 0; i < bits.size(); i++) {
			if (i * B + B > len) {
				bits[i] &= (1llu << (len % B)) - 1;
			}
			r += __builtin_popcountll(bits[i]);
		}
		return r;
	}

	int ctz() {
		int r = 0;
		for (int i = 0; i < bits.size(); i++) {
			if (bits[i] == 0) {
				continue;
			}
			int v = __builtin_ctzll(bits[i]);
			r = i * B + v;
			break;
		}
		if (r >= len) {
			r = len;
		}
		return r;
	}

	friend ostream& operator << (ostream &out, const Bitmask &bitmask) {
		out << bitmask.len << " ";
		for (int i = 0; i < bitmask.len; i++) {
			out << bitmask.get(i);
		}
		return out;
	}
};

}

#endif