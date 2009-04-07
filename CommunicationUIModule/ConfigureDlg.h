#ifndef incl_SetConfigureDlg_h
#define incl_SetConfigureDlg_h

#pragma once

//#include <stdafx.h>

#pragma warning( push )
#pragma warning( disable : 4250 )
#include <gtkmm.h>
#pragma warning( pop )


class IConfigureCallBack
{
public:
    IConfigureCallBack()  {}
    virtual ~IConfigureCallBack() {}
	virtual void Callback(std::string aConfigName, std::map<std::string, Foundation::Comms::SettingsAttribute> attributes) = 0;
};


class ConfigureDlg : public Gtk::Dialog
{
public:
	ConfigureDlg(int count, std::map<std::string, Foundation::Comms::SettingsAttribute> attributes, std::string name,
		IConfigureCallBack* aConfCaller);


	virtual ~ConfigureDlg(void);

protected:
	virtual void on_button_close();

  //Child widgets:
	Gtk::ScrolledWindow m_ScrolledWindow;
	Gtk::Table m_Table;
	Gtk::Button m_Button_Close;


	struct accessWidgets
	{
		Gtk::Label* label;
		Gtk::Entry* entry;
	};


	// MVC model part
	// did not know how to read widgets from table, so adding references to all entries for reading
	std::vector<accessWidgets*> widgetPairs;

	std::string configName;
	IConfigureCallBack* configCallback;
	//Foundation::Comms::CommunicationManagerServiceInterface* commManager;
	void (*callbackMethod)(std::map<std::string, Foundation::Comms::SettingsAttribute>);  
};

#endif
