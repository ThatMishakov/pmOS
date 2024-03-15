#pragma once
namespace std {
    
    class exception
	{
	public:
		exception() throw();
		exception(const exception&) throw();
		exception& operator=(const exception&) throw();
		virtual ~exception();
		virtual const char* what() const throw();
	};

}