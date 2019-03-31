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

class Matrix_t
{
public:
    // c-tor
    Matrix_t(Index_t nrows)
        : nrows_(nrows)
    {
        nrows_per_nodelet_ = nrows_ + nrows_ % NODELETS();

        // allocate Row_t's
        row_block_ = (Row_t **)mw_malloc2d(NODELETS(),
                                           nrows_per_nodelet_ * sizeof(Row_t));

        // placement new Row_t's
        for (Index_t irow = 0; irow < nrows_; ++irow)
        {
            size_t nid(n_map(irow));
            size_t rid(r_map(irow));

            // migrations to do placement new on other nodelets
            pRow_t rowPtr = new(row_block_[nid] + rid) Row_t();
            assert(rowPtr);
        }
    }

    // d-tor
    ~Matrix_t()
    {
        mw_free(row_block_);
    }

    void addRow(Index_t row_idx)
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

        pRow_t rowPtr = row_block_[n_map(row_idx)] + r_map(row_idx);

        for (Row_t::iterator it = tmpRow.begin(); it < tmpRow.end(); ++it)
        {
            rowPtr->push_back(*it);
        }
    }

    Index_t * nodelet_addr(Index_t i)
    {
        return (Index_t *)(row_block_ + n_map(i));
    }
    
private:
    Index_t nrows_;
    Index_t nrows_per_nodelet_;
    Row_t ** row_block_;
};


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

    Matrix_t A(nrows), B(nrows);

    Index_t row_idx_1 = 2; // row on nodelet 2
    cilk_migrate_hint(A.nodelet_addr(row_idx_1));
    cilk_spawn A.addRow(row_idx_1);

    Index_t row_idx_2 = 13; // row on nodelet 5
    cilk_migrate_hint(B.nodelet_addr(row_idx_2));
    cilk_spawn B.addRow(row_idx_2);
    cilk_sync;

//    cilk_migrate_hint(r + n_map(row_idx_1));
//    Scalar_t a = cilk_spawn dot(r, row_idx_1, row_idx_2);
//    cilk_sync;

//    assert(a == 3);

//    // profiler shows proper ping pong behavior between 2 and 5
//    /*
//      MEMORY MAP
//      336,2,2,0,0,1,0,0
//      0,0,2,0,0,0,0,0
//      3,0,1365,2,0,13,0,0
//      0,0,0,0,2,0,0,0
//      0,0,0,0,0,2,0,0
//      3,0,12,0,0,1321,2,0
//      0,0,0,0,0,0,0,2
//      2,0,0,0,0,0,0,0
//     */
    
    return 0;
}
