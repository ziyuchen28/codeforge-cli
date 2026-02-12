
#pragma once

namespace cli 
{

// If a subcommand handled argv, returns true and writes exit code into *out_rc.
// If not handled, returns false and main should continue with normal program flow.
bool handle(int argc, char **argv, int *out_rc);

} // namespace cli
