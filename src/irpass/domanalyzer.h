/*
# dominance analyzer
====================

*/

#ifndef __CPL_DOM_ANALYZER_H__
#define __CPL_DOM_ANALYZER_H__

#include <vector>
#include <map>
#include <set>

#include "../ir.h"
#include "../bitmask.h"
#include "cfgbuilder.h"


namespace IR {

namespace Passes {

using namespace std;
using namespace IR;
using namespace Bits;


class DomAnalyzer : public Pass {
public:
	CFGBuilder cfgBuilder;

	Module *module;

	static map <BasicBlock *, BasicBlock *> domParent;
	static map <BasicBlock *, set <BasicBlock *>> domChildren;
	static map <BasicBlock *, set <BasicBlock *>> dom;
	static map <BasicBlock *, set <BasicBlock *>> domFrontier;
	static map <BasicBlock *, int> domDepth;

	BasicBlock *domTreeLCA(BasicBlock *u, BasicBlock *v) {
		while (domDepth.count(u) && domDepth.count(v) && u != v) {
			if (domDepth[u] < domDepth[v]) {
				swap(u, v);
			}
			u = domParent[u];
		}
		if (u != v) {
			return nullptr;
		}
		return u;
	}

	void setDomDepth(BasicBlock *block, int depth = 0) {
		domDepth[block] = ++depth;
		for (BasicBlock *child : domChildren[block]) {
			setDomDepth(child, depth);
		}
	}

	void visitFunction(Function *node) {
		map <int, int> index;
		vector <Bitmask> dom;
		index.clear();
		int cnt = 0;
		vector <BasicBlock *> blocks;
		for (BasicBlock *block : node->blocks) {
			index[block->id] = cnt++;
			blocks.emplace_back(block);
		}
		dom.resize(cnt);
		for (int i = 0; i < cnt; i++) {
			dom[i].resize(cnt);
			dom[i].fill();
		}

		// build dominator tree
		bool changed = true;
		while (changed) {
			changed = false;
			for (int i = 0; i < cnt; i++) {
				Bitmask curDom(cnt);
				if (i > 0) {
					curDom.fill();
				}
				for (BasicBlock *block : blocks[i]->jumpFrom) {
					curDom.bitwiseAnd(dom[index[block->id]]);
				}
				curDom.set(i, 1);
				if (curDom.count() != dom[i].count()) {
					changed = true;
				}
				dom[i] = curDom;
			}
		}

		for (int i = 0; i < cnt; i++) {
			for (int j = 0; j < cnt; j++) {
				if (dom[i].get(j)) {
					this->dom[blocks[i]].insert(blocks[j]);
				}
			}
		}

		for (int i = 0; i < cnt; i++) {
			BasicBlock *block = blocks[i];
			domParent[block] = nullptr;
			if (blocks[i]->jumpFrom.size() == 0) {
				continue;
			}
			for (int j = 0; j < cnt; j++) {
				if (dom[i].get(j) > 0 && dom[j].count() + 1 == dom[i].count()) {
					domParent[block] = blocks[j];
					break;
				}
			}
			if (domParent[block] != nullptr && domParent[block] != block) {
				domChildren[domParent[block]].insert(block);
			}
		}

		// calculate dominance frontier
		for (int i = 0; i < cnt; i++) {
			for (BasicBlock *block : blocks[i]->jumpFrom) {
				BasicBlock *cur = block;
				while (cur != nullptr && (this->dom[blocks[i]].count(cur) == 0 || blocks[i] == cur)) {
					domFrontier[cur].insert(blocks[i]);
					cur = domParent[cur];
				}
			}
		}

		setDomDepth(blocks[0]);
	}

	void visitModule(Module *node) {
		node->accept(cfgBuilder);

		module = node;

		domParent.clear();
		domChildren.clear();
		dom.clear();
		domFrontier.clear();
		domDepth.clear();
		for (Function *func : node->funcs) {
			func->accept(*this);
		}
	}
};

map <BasicBlock *, BasicBlock *> DomAnalyzer::domParent;
map <BasicBlock *, set <BasicBlock *>> DomAnalyzer::domChildren;
map <BasicBlock *, set <BasicBlock *>> DomAnalyzer::dom;
map <BasicBlock *, set <BasicBlock *>> DomAnalyzer::domFrontier;
map <BasicBlock *, int> DomAnalyzer::domDepth;

}

}

#endif