module std::core::string::iterator;



struct StringIterator
{
	char[] utf8;
	usize current;
}

fn void StringIterator.reset(StringIterator* this)
{
	this.current = 0;
}

fn Char32! StringIterator.next(StringIterator* this)
{
	usize len = this.utf8.len;
	usize current = this.current;
	if (current >= len) return IteratorResult.NO_MORE_ELEMENT!;
	usize read = (len - current < 4 ? len - current : 4);
	Char32 res = conv::utf8_to_char32(&this.utf8[current], &read)?;
	this.current += read;
	return res;
}