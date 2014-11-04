#include "liquid.h"
#include "tokenizer.h"
#include <stdio.h>

static ID
    intern_raise_missing_variable_terminator,
    intern_raise_missing_tag_terminator,
    intern_nodelist,
    intern_blank,
    intern_is_blank,
    intern_clear,
    intern_tags,
    intern_parse,
    intern_square_brackets;

static int is_id(int c)
{
    return rb_isalnum(c) || c == '_';
}

inline static const char *read_while(const char *start, const char *end, int (func)(int))
{
    while (start < end && func((unsigned char) *start)) start++;
    return start;
}

static VALUE rb_block_parse(VALUE self, VALUE tokens, VALUE options)
{
    tokenizer_t *tokenizer;
    Tokenizer_Get_Struct(tokens, tokenizer);

    rb_ivar_set(self, intern_blank, Qtrue);

    token_t token;
    VALUE tags = Qnil;
    VALUE nodelist = rb_ivar_get(self, intern_nodelist);

    while (true) {
        tokenizer_next(tokenizer, &token);

        switch (token.type) {
            case TOKENIZER_TOKEN_NONE:
                return rb_yield_values(2, Qnil, Qnil);

            case TOKEN_INVALID:
            {
                VALUE str = rb_enc_str_new(token.str, token.length, utf8_encoding);

                ID raise_method_id = intern_raise_missing_variable_terminator;
                if (token.str[1] == '%') raise_method_id = intern_raise_missing_tag_terminator;

                return rb_funcall(self, raise_method_id, 2, str, options);
            }
            case TOKEN_RAW:
            {
                VALUE str = rb_enc_str_new(token.str, token.length, utf8_encoding);
                rb_ary_push(nodelist, str);

                if (rb_ivar_get(self, intern_blank) == Qtrue) {
                    const char *end = token.str + token.length;

                    if (read_while(token.str, end, rb_isspace) < end)
                        rb_ivar_set(self, intern_blank, Qfalse);
                }
                break;
            }
            case TOKEN_VARIABLE:
            {
                VALUE args[2] = {rb_enc_str_new(token.str + 2, token.length - 4, utf8_encoding), options};
                VALUE var = rb_class_new_instance(2, args, cLiquidVariable);
                rb_ary_push(nodelist, var);
                rb_ivar_set(self, intern_blank, Qfalse);
                break;
            }
            case TOKEN_TAG:
            {
                const char *start = token.str + 2, *end = token.str + token.length - 2;

                // Imitate \s*(\w+)\s*(.*)? regex
                const char *name_start = read_while(start, end, rb_isspace);
                const char *name_end = read_while(name_start, end, is_id);

                VALUE tag_name = rb_enc_str_new(name_start, name_end - name_start, utf8_encoding);

                if (tags == Qnil)
                    tags = rb_funcall(cLiquidTemplate, intern_tags, 0);

                VALUE tag_class = rb_funcall(tags, intern_square_brackets, 1, tag_name);

                const char *markup_start = read_while(name_end, end, rb_isspace);
                VALUE markup = rb_enc_str_new(markup_start, end - markup_start, utf8_encoding);

                if (tag_class == Qnil)
                    return rb_yield_values(2, tag_name, markup);

                VALUE new_tag = rb_funcall(tag_class, intern_parse, 4, tag_name, markup, tokens, options);

                if (rb_ivar_get(self, intern_blank) == Qtrue && !RTEST(rb_funcall(new_tag, intern_is_blank, 0)))
                    rb_ivar_set(self, intern_blank, Qfalse);

                rb_ary_push(nodelist, new_tag);
                break;
            }
        }
    }
    return Qnil;
}

void init_liquid_block()
{
    intern_raise_missing_variable_terminator = rb_intern("raise_missing_variable_terminator");
    intern_raise_missing_tag_terminator = rb_intern("raise_missing_tag_terminator");
    intern_nodelist = rb_intern("@nodelist");
    intern_blank = rb_intern("@blank");
    intern_is_blank = rb_intern("blank?");
    intern_clear = rb_intern("clear");
    intern_tags = rb_intern("tags");
    intern_parse = rb_intern("parse");
    intern_square_brackets = rb_intern("[]");

    VALUE cLiquidBlockBody = rb_const_get(mLiquid, rb_intern("BlockBody"));
    rb_define_method(cLiquidBlockBody, "c_parse", rb_block_parse, 2);
}

