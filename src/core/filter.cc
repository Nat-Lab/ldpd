#include "core/filter.hh"

namespace ldpd {

RouteFilter::RouteFilter(const Prefix &sourcePrefix, FilterMatchType type, FilterAction action, uint8_t upto) : source(sourcePrefix) {
    this->type = type;
    this->action = action;
    this->upto = upto;
}

FilterAction RouteFilter::apply(const Prefix &target) const {
    if (type == FilterMatchType::Exact) {
        return target.isInSameNetwork(source) ? action : FilterAction::Nop;
    }

    if (type == FilterMatchType::UpTo) {
        return (source.getNetwork() == target.maskWith(source.len) && target.len <= source.len) ? action : FilterAction::Nop;
    }

    // should be unreached
    return FilterAction::Nop;
}

RoutePolicy::RoutePolicy(FilterAction defaultAction) : terms() {
    this->defaultAction = defaultAction;
}

void RoutePolicy::addFilter(const RouteFilter &filter) {
    terms.push_back(filter);
}

FilterAction RoutePolicy::apply(const Prefix &target) const {
    for (const RouteFilter &term : terms) {
        FilterAction action = term.apply(target);

        if (action != FilterAction::Nop) {
            return action;
        }
    }

    return defaultAction;
}

}