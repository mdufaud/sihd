#ifndef __SIHD_UTIL_CONFIGURABLE_HPP__
# define __SIHD_UTIL_CONFIGURABLE_HPP__

# include <sihd/util/datatype.hpp>
# include <sihd/util/Callback.hpp> 
# include <functional>

namespace sihd::util
{

class Configurable
{
    public:
        Configurable() {};
        virtual ~Configurable() {};

		template <class C, typename T>
		void	add_conf(const std::string & name, bool (C::*method)(T))
		{
			_callbackManager.set<C, bool, T>(name, dynamic_cast<C *>(this), method);
		};

		template <typename T>
		void	add_conf(const std::string & name, std::function<bool(T)> fun)
		{
			_callbackManager.set<bool, T>(name, fun);
		};

		template <typename T1, typename T2>
		void	add_conf(const std::string & name, std::function<bool(T1, T2)> fun)
		{
			_callbackManager.set<bool, T1, T2>(name, fun);
		};

		template <typename T>
		bool	set_conf(const std::string & name, T param)
		{
			return _callbackManager.call<bool, T>(name, param);
		}

		template <typename ... T>
		bool	set_conf(const std::string & name, T... params)
		{
			return _callbackManager.call<bool, T...>(name, params...);
		}

		bool	set_conf_float(const std::string & name, double param)
		{
			try
			{	
				return _callbackManager.call<bool, double>(name, param);
			}
			catch (const std::invalid_argument & e)
			{
				return _callbackManager.call<bool, float>(name, (float)param);
			}
		}

		bool	set_conf_int(const std::string & name, int64_t param)
		{
			try { return _callbackManager.call<bool, int64_t>(name, param); }
			catch (const std::invalid_argument & e) {}
			try { return _callbackManager.call<bool, uint64_t>(name, param); }
			catch (const std::invalid_argument & e) {}
			try { return _callbackManager.call<bool, int32_t>(name, param); }
			catch (const std::invalid_argument & e) {}
			try { return _callbackManager.call<bool, uint32_t>(name, param); }
			catch (const std::invalid_argument & e) {}
			try { return _callbackManager.call<bool, int16_t>(name, param); }
			catch (const std::invalid_argument & e) {}
			try { return _callbackManager.call<bool, uint16_t>(name, param); }
			catch (const std::invalid_argument & e) {}
			try { return _callbackManager.call<bool, int8_t>(name, param); }
			catch (const std::invalid_argument & e) {}

			return _callbackManager.call<bool, uint8_t>(name, param);
		}

		bool	set_conf_str(const std::string & name, std::string param)
		{
			try
			{	
				return _callbackManager.call<bool, std::string>(name, param);
			}
			catch (const std::invalid_argument & e)
			{
				return _callbackManager.call<bool, const char *>(name, param.c_str());
			}
		}

    private:
		CallbackManager _callbackManager;
};

}

#endif 