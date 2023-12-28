/*
# loop analyzer
===============

*/

#ifndef __CPL_LOOP_ANALYZER_H__
#define __CPL_LOOP_ANALYZER_H__

#include <vector>
#include <map>
#include <set>

#include "../ir.h"
#include "domanalyzer.h"


namespace IR {

using namespace std;


class Loop {
public:
	BasicBlock *header = nullptr;
	set <BasicBlock *> body;
	set <pair <BasicBlock *, BasicBlock *>> exits;

	Loop() {}
	Loop(BasicBlock *header) {
		this->header = header;
	}
};


namespace Passes {

using namespace std;
using namespace IR;


class LoopAnalyzer : public Pass {
public:
	DomAnalyzer domAnalyzer;

	Module *module;

	static vector <Loop *> loops;

	static map <BasicBlock *, int> loopDepth;
	static map <BasicBlock *, vector <Loop *>> loopAsHeader;

	map <BasicBlock *, bool> visited;
	vector <BasicBlock *> visitStack;
	map <BasicBlock *, BasicBlock *> component;

	BasicBlock *getComponent(BasicBlock *block) {
		if (component.count(block) == 0) {
			return block;
		}
		component[block] = getComponent(component[block]);
		return component[block];
	}

	void markLoop(BasicBlock *block, BasicBlock *header, Loop *loop) {
		if (loop->body.count(block)) {
			return;
		}
		loop->body.insert(block);
		if (getComponent(block) != getComponent(header)) {
			component[getComponent(block)] = header;
		}
		if (block == header) {
			return;
		}
		for (BasicBlock *from : block->jumpFrom) {
			markLoop(from, header, loop);
		}
	}

	void findLoops(BasicBlock *block) {
		visited[block] = true;
		visitStack.emplace_back(block);
		for (BasicBlock *to : block->jumpTo) {
			if (!visited[to]) {
				findLoops(to);
				continue;
			}
			// there is an edge to ancestor
			Loop *loop = new Loop(to);
			markLoop(block, to, loop);
			BasicBlock *lca = block;
			for (BasicBlock *body : loop->body) {
				lca = domAnalyzer.domTreeLCA(lca, body);
			}
			if (lca != to) {
				delete loop;
				continue;
			}
			// finds a loop
			for (BasicBlock *body : loop->body) {
				for (BasicBlock *dest : body->jumpTo) {
					if (loop->body.count(dest)) {
						continue;
					}
					if (getComponent(dest) == getComponent(body)) {
						continue;
					}
					loop->exits.insert({body, dest});
				}
			}
			loops.emplace_back(loop);
			loopAsHeader[to].emplace_back(loop);
		}
		visitStack.pop_back();
	}

	void setLoopDepth(BasicBlock *block) {
		set <BasicBlock *> descendants;
		if (loopAsHeader.count(block)) {
			loopDepth[block]++;
		}
		for (Loop *loop : loopAsHeader[block]) {
			for (BasicBlock *desc : loop->body) {
				if (descendants.count(desc)) {
					continue;
				}
				descendants.insert(desc);
				loopDepth[desc] = max(loopDepth[desc], loopDepth[block]);
			}
		}
		for (BasicBlock *child : domAnalyzer.domChildren[block]) {
			setLoopDepth(child);
		}
	}

	void visitFunction(Function *node) {
		visited.clear();
		visitStack.clear();
		component.clear();
		BasicBlock *entry = node->blocks.first();
		findLoops(entry);
		setLoopDepth(entry);
	}

	void visitModule(Module *node) {
		node->accept(domAnalyzer);

		module = node;

		for (Loop *loop : loops) {
			delete loop;
		}
		loops.clear();
		loopDepth.clear();
		loopAsHeader.clear();
		for (Function *func : node->funcs) {
			func->accept(*this);
		}
	}
};

vector <Loop *> LoopAnalyzer::loops;

map <BasicBlock *, int> LoopAnalyzer::loopDepth;
map <BasicBlock *, vector <Loop *>> LoopAnalyzer::loopAsHeader;

}

}

#endif