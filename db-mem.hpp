#ifndef DB_MEM_HPP
#define DB_MEM_HPP

// implement an in-memory db.
// with its transaction data also in memory.
class db_mem{
public:
    db_mem() : db(), transactionData_() {
    }

    ~db_mem() {

    }

    bool find(const blob_t &key, string &value) const {
        if(transactionData_) {
            if(contains(transactionData_->adds, key)) {
                value= transactionData_->adds[key];
                return true;
            } else if(contains(transactionData_->deletes, key)) {
                return false;
            }
        }
        if(contains(db, key)) {
            value = db[key];
            return true;
        }
        return false;
    }

    bool has_key(const blob_t &key) const {
        string xxx;
        return find(key, xxx);
    }

    void add(const blob_t &key,
             const blob_t &value,
             bool replace) {
        if(transactionData_) {
            if(has_key(key) && !replace) {
                return;
            }
            transactionData_->adds[key] = value;
            transactionData_->deletes.erase(key);
        } else {
            if(contains(db, key) && !replace) {
                return;
            }
            db[key] = value;
        }
    }

    void delete_key(const blob_t &key) {
        if(transactionData_) {
            transactionData_->adds.erase(key);
            if(contains(db, key)) {
                transactionData_->deletes.insert(k);
            }
        } else {
            db.erase(key);
        }
    }

    void begin_transaction() {
        assert(!transactionData_);
        transactionData_.reset(new TransactionData());
    }

    void end_transaction() {
        assert(transactionData_);
        foreach(i, transactionData_->adds) {
            db_[i->first] = i->second;
        }
        foreach(i, transactionData_->deletes) {
            db_.erase(*i);
        }
        transactionData_.reset();
    }

    void abort_transaction() {
        assert(transactionData_);
        transactionData_.reset();
    }

    bool in_transaction() const {
        return transactionData_;
    }

    // what? can't clear in a transaction?
    void clear() {
        if(transactionData_) {
            transactionData_->adds.clear();
            transactionData_->deletes.clear();
            foreach(it, db) {
                transactionData_->deletes.add(it->first);
            }
        } else {
            db.clear();
        }
    }

    // return false if:
    // 1) there are adds (replace?)
    // 2) if there is at least one in db but not in deletes
    bool empty() const {
        if(transactionData_) {
            if(!transactionData_->adds.empty()) {
                return false;
            }
            foreach(it, db) {
                if(!contains(transactionData_, it->first)) {
                    return false;
                }
            }
        }
        return db.empty();
    }

    // return sizeof(db) + sizeof(db-adds) - sizeof(deletes)
    size_t size() const {
       size_t rv = db.size();
       if(transactionData_)  {
           foreach(it, transactionData_->adds) {
               if(!contains(db, it->first)) {
                   rv++;
               }
           }
           rv -= (transactionData_->deletes).size();
       }
       return rv;
    }

protected:

    // it will be iterates (adds + db - deletes)
    // to make it simple, output adds first, then db.
    friend struct value_iterator {
        value_iterator(db_mem* impl, bool &isValid) 
            : db(impl->db),
            db_it(db.begin()) {
                if(impl->transactionData_) {
                    adds_it = (impl->transactionData_->adds).begin();
                }
                if(!inc(true)) {
                    isValid = false;
                } else {
                    isValid = true;
                }
        }

        bool inc(bool skip) {
            if(impl->transactionData_) {
                map<string>::const_iterator adds_end = (impl->transactionData_->adds).end();
                if(adds_it != adds_end) {
                    if(!skip) {
                        ++adds_it;
                    } 
                    if((it = adds_it) != adds_end) {
                        return true;
                    }
                } 
            }
            if(!skip) {
                ++db_it;
            }
            return (it == db_it) != db.end();
        }

    private:
        const map<string, string> &db;

        map<string, string>::const_iterator db_it;
        map<string, string>::const_iterator adds_it;

        // it points to either db_it or adds_it
        map<string, string>::const_iterator it;
    };

    key_iterator_impl_base *key_begin_ptr() const;

    key_value_iterator_impl_base *key_value_begin_ptr() const;

private:
    map<string, string> db_;

    // journal
    struct TransactionData {
        map<string, string> adds;
        set<string> deletes;
    };

    // Set if in a transaction; contains enough information to commit
    // or abort the transaction.
    scoped_ptr<TransactionData> transactionData_;
};
#endif /* DB_MEM_HPP */
