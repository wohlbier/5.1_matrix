/*
  MM = MEMORY MAP
  Memory Reference Map. Entry (i,j) is the number of references
  made by threads on nodelet i to nodelet j. i == j for local references.
  Sum of column j is number of references on nodelet j, excluding remotes.
  Sum of row i, excluding i == j, is number of migrations away from nodelet i.

  Each row in the memory map represents the source nodelet on which a memory
  instruction was encountered and the column represents the destination nodelet
  on which the memory instruction was ultimately executed. Values on the
  diagonal represent local memory accesses and non-diagonal values show the
  number of memory accesses that required a migration to another nodelet to
  complete.

  RM = REMOTES MAP
  Remotes Reference Map. Counts number of remote operations from
  threads on nodelet i to nodelet j.

  Each row in the remotes map is a source nodelet and each column is a
  destination nodelet for a remote memory operation. Note that the diagonal
  values are always zero because a remote update to a local value will be
  counted as a local memory operation. In this example, all the values are zero
  showing that no remote memory operations occurred.
 */
#include <assert.h>
#include <tuple>
#include <vector>

#include <cilk.h>
#include <memoryweb.h>

typedef long Index_t;
typedef long Scalar_t;
typedef std::vector<std::tuple<Index_t,Scalar_t>> Row_t;
typedef Row_t * pRow_t;

static inline
Index_t r_map(Index_t i) { return i / NODELETS(); } // slow running index
static inline
Index_t n_map(Index_t i) { return i % NODELETS(); } // fast running index


Row_t ** allocSparseRows(Index_t nrows, Index_t nrows_per_nodelet)
{
    // NB: r is a "view 2" pointer to a Row_t
    //     r + 2 is an address on nodelet 2.
    //     r[2] will migrate to node 2 and read the value at address r + 2

    // MM: 124 memory references on nodelet 0.
    // RM: 0+1 on all j nodelets (0,j) j=1...7
    Row_t ** r
        = (Row_t **)mw_malloc2d(NODELETS(),
                                nrows_per_nodelet * sizeof(Row_t));

    for (Index_t row_idx = 0; row_idx < nrows; ++row_idx)
    {
        size_t nid(n_map(row_idx));
        size_t rid(r_map(row_idx));

        // r_repl[nid][rid] is a Row_t
        // r_repl[nid] + rid is address of Row_t's
        // &r_repl[nid][rid] = r_repl[nid] + rid

        // migrations to do placement new on other nodelets
        pRow_t rowPtr = new(r[nid] + rid) Row_t();
    }

    return r;
}

void addRow(Row_t ** r, Index_t row_idx)
{
    Row_t tmpRow;
    if (row_idx % 2 == 0)
    {
        tmpRow.push_back(std::make_tuple(0,1));
        tmpRow.push_back(std::make_tuple(3,1));
        tmpRow.push_back(std::make_tuple(5,1));
        tmpRow.push_back(std::make_tuple(7,1));
        tmpRow.push_back(std::make_tuple(12,1));
        tmpRow.push_back(std::make_tuple(14,1));
        tmpRow.push_back(std::make_tuple(27,1));
        tmpRow.push_back(std::make_tuple(31,1));
    }
    else
    {
        tmpRow.push_back(std::make_tuple(1,1));
        tmpRow.push_back(std::make_tuple(7,1));
        tmpRow.push_back(std::make_tuple(10,1));
        tmpRow.push_back(std::make_tuple(14,1));
        tmpRow.push_back(std::make_tuple(18,1));
        tmpRow.push_back(std::make_tuple(27,1));
        tmpRow.push_back(std::make_tuple(28,1));
    }

    pRow_t rowPtr = r[n_map(row_idx)] + r_map(row_idx);

    for (Row_t::iterator it = tmpRow.begin(); it < tmpRow.end(); ++it)
    {
        rowPtr->push_back(*it);
    }
}

Scalar_t dot(Row_t ** r, Index_t a_row_idx, Index_t b_row_idx)
{

    pRow_t a = r[n_map(a_row_idx)] + r_map(a_row_idx);
    pRow_t b = r[n_map(b_row_idx)] + r_map(b_row_idx);

    Row_t::iterator ait = a->begin();
    Row_t::iterator bit = b->begin();

    Scalar_t result = 0;

    while (ait != a->end() && bit != b->end())
    {
        Index_t a_idx = std::get<0>(*ait);
        Index_t b_idx = std::get<0>(*bit);

        if (a_idx == b_idx)
        {
            result += std::get<1>(*ait) * std::get<1>(*bit);
            ++ait;
            ++bit;
        }
        else if (a_idx < b_idx)
        {
            ++ait;
        }
        else
        {
            ++bit;
        }
    }

    return result;
}

int main(int argc, char* argv[])
{
    starttiming();

    Index_t nrows = 16;
    Index_t nrows_per_nodelet = nrows + nrows % NODELETS();

    Row_t ** r = cilk_spawn allocSparseRows(nrows, nrows_per_nodelet);
    cilk_sync;

    Index_t row_idx_1 = 2; // row on nodelet 2
    cilk_migrate_hint(r + n_map(row_idx_1));
    cilk_spawn addRow(r, row_idx_1);

    Index_t row_idx_2 = 13; // row on nodelet 5
    cilk_migrate_hint(r + n_map(row_idx_2));
    cilk_spawn addRow(r, row_idx_2);
    cilk_sync;

    // up to this point, no migrations between nodelets 2 and 5
    
    /*
      MEMORY MAP
      281,2,1,0,0,1,0,0
      0,0,2,0,0,0,0,0
      2,0,1315,2,0,0,0,0
      0,0,0,0,2,0,0,0
      0,0,0,0,0,2,0,0
      2,0,0,0,0,1272,2,0
      0,0,0,0,0,0,0,2
      2,0,0,0,0,0,0,0
    */

    cilk_migrate_hint(r + n_map(row_idx_1));
    Scalar_t a = cilk_spawn dot(r, row_idx_1, row_idx_2);
    cilk_sync;

    assert(a == 3);

    // profiler shows proper ping pong behavior between 2 and 5
    /*
      MEMORY MAP
      336,2,2,0,0,1,0,0
      0,0,2,0,0,0,0,0
      3,0,1365,2,0,13,0,0
      0,0,0,0,2,0,0,0
      0,0,0,0,0,2,0,0
      3,0,12,0,0,1321,2,0
      0,0,0,0,0,0,0,2
      2,0,0,0,0,0,0,0
     */
    
    return 0;
}
