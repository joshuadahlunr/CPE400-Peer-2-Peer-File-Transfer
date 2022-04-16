#ifndef __DIFF_HPP__
#define __DIFF_HPP__

#include <dtl/dtl.hpp>

#include "include_everywhere.hpp"

namespace dtl {
	dtl_typedefs(char, std::string)

	inline static std::string uniPatch (dtl::uniHunkVec uniHunks, const std::string& seq) {
		static auto joinSesVec = [](sesElemVec& s1, const sesElemVec& s2) {
			if (!s2.empty()) {
				for (auto vit=s2.begin();vit!=s2.end();++vit) {
					s1.push_back(*vit);
				}
			}
		};

		dtl::elemList        seqLst(seq.begin(), seq.end());
		dtl::sesElemVec      shunk;
		dtl::sesElemVec_iter vsesIt;
		dtl::elemList_iter   lstIt         = seqLst.begin();
		long long       inc_dec_total = 0;
		long long       gap           = 1;
		for (auto it=uniHunks.begin();it!=uniHunks.end();++it) {
			joinSesVec(shunk, it->common[0]);
			joinSesVec(shunk, it->change);
			joinSesVec(shunk, it->common[1]);
			it->a         += inc_dec_total;
			inc_dec_total += it->inc_dec_count;
			for (long long i=0;i<it->a - gap;++i) {
				++lstIt;
			}
			gap = it->a + it->b + it->inc_dec_count;
			vsesIt = shunk.begin();
			while (vsesIt!=shunk.end()) {
				switch (vsesIt->second.type) {
				case dtl::SES_ADD :
					seqLst.insert(lstIt, vsesIt->first);
					break;
				case dtl::SES_DELETE :
					if (lstIt != seqLst.end()) {
						lstIt = seqLst.erase(lstIt);
					}
					break;
				case dtl::SES_COMMON :
					if (lstIt != seqLst.end()) {
						++lstIt;
					}
					break;
				default :
					// no fall-through
					break;
				}
				++vsesIt;
			}
			shunk.clear();
		}

		std::string patchedSeq(seqLst.begin(), seqLst.end());
		return patchedSeq;
	}

	template <typename sesElem>
	inline uniHunk<sesElem> invert(uniHunk<sesElem> hunk) {
		for(auto& [_, elem]: hunk.change){
			if(elem.type == SES_DELETE) elem.type = SES_ADD;
			else if(elem.type == SES_ADD) elem.type = SES_DELETE;
		}

		return hunk;
	}

	inline uniHunkVec& invert(uniHunkVec& hunks) {
		for(auto& hunk: hunks)
			hunk = invert(hunk);
		return hunks;
	}
}



inline dtl::uniHunkVec extractDiff(const std::string& original, const std::string& changed) {
	dtl::Diff<char, std::string> diff(original, changed);
	diff.compose();
	diff.composeUnifiedHunks();
	return diff.getUniHunks();
}

inline std::string applyDiff(const std::string& original, const dtl::uniHunkVec& diff) {
	return uniPatch(diff, original);
}

inline std::string undoDiff(const std::string& changed, dtl::uniHunkVec diff) {
	return uniPatch(dtl::invert(diff), changed);
}

#endif // __DIFF_HPP__