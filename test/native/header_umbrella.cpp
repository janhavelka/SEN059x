#include <SEN059x/SEN059x.h>

static_assert(SEN059x::Ok().ok());
static_assert(SEN059x::RequestContext{}.addressPolicy == SEN059x::AddressPolicy::StandardUnicast);
static_assert(SEN059x::VERSION[0] != '\0');
