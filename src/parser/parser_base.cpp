
#include "parser/parser_base.hpp"
#include "parser/parser.hpp"

namespace Myre
{
    ParseContext &ParserBase::context()
    {
        return parser_->get_context();
    }

    ErrorNode *ParserBase::create_error(const char *msg)
    {
        return parser_->create_error(msg);
    }

} // namespace Myre