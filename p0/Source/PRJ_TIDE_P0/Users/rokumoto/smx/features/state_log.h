#include "../configure.h"

namespace app {

//~ @feature StateLog
struct StateLog {
    string_type state_;

    const string_type& state() const
    {
        return state_;
    }

    void set_state(const string_type& state) //~ @action
    {
        state_ = state;
    }
};

} // namespace app