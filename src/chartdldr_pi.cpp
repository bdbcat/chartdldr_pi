/******************************************************************************
 * $Id: chartdldr_pi.cpp,v 1.0 2011/02/26 01:54:37 nohal Exp $
 *
 * Project:  OpenCPN
 * Purpose:  Chart downloader Plugin
 * Author:   Pavel Kalian
 *
 ***************************************************************************
 *   Copyright (C) 2011 by Pavel Kalian   *
 *   $EMAIL$   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */


#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include "chartdldr_pi.h"
#include <wx/url.h>
#include <wx/progdlg.h>
#include <wx/sstream.h>
#include <wx/wfstream.h>
#include <wx/filename.h>
#include <wx/listctrl.h>
#include <wx/dir.h>
#include <wx/filesys.h>
#include <wx/zipstrm.h>
#include <wx/wfstream.h>
#include <memory>
#include <wx/protocol/http.h>

#include <wx/arrimpl.cpp>
    WX_DEFINE_OBJARRAY(wxArrayOfChartSources);


// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr)
{
    return new chartdldr_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p)
{
    delete p;
}

//---------------------------------------------------------------------------------------------------------
//
//    ChartDldr PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------

#include "icons.h"

//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

chartdldr_pi::chartdldr_pi(void *ppimgr)
      :opencpn_plugin_19(ppimgr)
{
      // Create the PlugIn icons
      initialize_images();

      m_chartSources = NULL;
      m_parent_window = NULL;
      m_pChartCatalog = NULL;
      m_pChartSource = NULL;
      m_pconfig = NULL;
      m_leftclick_tool_id = -1;
      m_schartdldr_sources = wxEmptyString;
}

int chartdldr_pi::Init(void)
{
      AddLocaleCatalog( _T("opencpn-chartdldr_pi") );

      //    Get a pointer to the opencpn display canvas, to use as a parent for the POI Manager dialog
      m_parent_window = GetOCPNCanvasWindow();

      //    Get a pointer to the opencpn configuration object
      m_pconfig = GetOCPNConfigObject();
      m_pOptionsPage = NULL;

      m_chartSources = new wxArrayOfChartSources();
      m_pChartCatalog = new ChartCatalog;
      m_pChartSource = NULL;

      //    And load the configuration items
      LoadConfig();

      wxStringTokenizer st(m_schartdldr_sources, _T("|"), wxTOKEN_DEFAULT);
      while ( st.HasMoreTokens() )
      {
            wxString s1 = st.GetNextToken();
            wxString s2 = st.GetNextToken();
            wxString s3 = st.GetNextToken();
            m_chartSources->Add(new ChartSource(s1, s2, s3));
      }

      return (
              WANTS_PREFERENCES         |
              WANTS_CONFIG              |
              INSTALLS_TOOLBOX_PAGE
           );
}

bool chartdldr_pi::DeInit(void)
{
      m_chartSources->Clear();
      wxDELETE(m_chartSources);
      wxDELETE(m_pChartCatalog);
      wxDELETE(m_pChartSource);
      /* TODO: Seth */
//      dialog->Close();
//      dialog->Destroy();
//      wxDELETE(dialog);
      /* We must delete remaining page if the plugin is disabled while in Options dialog */
      if ( m_pOptionsPage )
      {
            if ( DeleteOptionsPage( m_pOptionsPage ) )
                  m_pOptionsPage = NULL;
            // TODO: any other memory leak?
      }
      return true;
}

int chartdldr_pi::GetAPIVersionMajor()
{
      return MY_API_VERSION_MAJOR;
}

int chartdldr_pi::GetAPIVersionMinor()
{
      return MY_API_VERSION_MINOR;
}

int chartdldr_pi::GetPlugInVersionMajor()
{
      return PLUGIN_VERSION_MAJOR;
}

int chartdldr_pi::GetPlugInVersionMinor()
{
      return PLUGIN_VERSION_MINOR;
}

wxBitmap *chartdldr_pi::GetPlugInBitmap()
{
      return _img_chartdldr_pi;
}

wxString chartdldr_pi::GetCommonName()
{
      return _("ChartDownloader");
}


wxString chartdldr_pi::GetShortDescription()
{
      return _("Chart Downloader PlugIn for OpenCPN");
}

wxString chartdldr_pi::GetLongDescription()
{
      return _("Chart Downloader PlugIn for OpenCPN\n\
Manages chart downloads and updates from sources supporting\n\
NOAA Chart Catalog format");
}

void chartdldr_pi::OnSetupOptions(void)
{
      m_pOptionsPage = AddOptionsPage( PI_OPTIONS_PARENT_CHARTS, _("Chart Downloader") );
      if (! m_pOptionsPage) {
            wxLogMessage( _T("Error: chartdldr_pi::OnSetupOptions AddOptionsPage failed!") );
            return;
      }
      wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );
      m_pOptionsPage->SetSizer( sizer );

      /* TODO: Seth */
      m_dldrpanel = new ChartDldrPanelImpl( this, m_pOptionsPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE );

      sizer->Add( m_dldrpanel, 0, wxALL | wxEXPAND );
      m_dldrpanel->SelectCatalog(m_selected_source);
      m_dldrpanel->SetSource(m_selected_source);
}

void chartdldr_pi::OnCloseToolboxPanel(int page_sel, int ok_apply_cancel)
{
      /* TODO: Seth */
      m_selected_source = m_dldrpanel->GetSelectedCatalog();
      SaveConfig();
}

bool chartdldr_pi::LoadConfig(void)
{
      wxFileConfig *pConf = (wxFileConfig *)m_pconfig;

      if(pConf)
      {
            pConf->SetPath ( _T ( "/Settings/ChartDnldr" ) );
            pConf->Read ( _T ( "Sources" ), &m_schartdldr_sources, wxEmptyString );
            pConf->Read ( _T ( "Source" ), &m_selected_source, -1 );
            return true;
      }
      else
            return false;
}

bool chartdldr_pi::SaveConfig(void)
{
      wxFileConfig *pConf = (wxFileConfig *)m_pconfig;

      m_schartdldr_sources.Clear();

      for (size_t i = 0; i < m_chartSources->GetCount(); i++)
      {
            ChartSource *cs = m_chartSources->Item(i);
            m_schartdldr_sources.Append(wxString::Format(_T("%s|%s|%s|"), cs->GetName().c_str(), cs->GetUrl().c_str(), cs->GetDir().c_str()));
      }

      if(pConf)
      {
            pConf->SetPath ( _T ( "/Settings/ChartDnldr" ) );
            pConf->Write ( _T ( "Sources" ), m_schartdldr_sources );
            pConf->Write ( _T ( "Source" ), m_selected_source );

            return true;
      }
      else
            return false;
}

void chartdldr_pi::ShowPreferencesDialog( wxWindow* parent )
{
}

ChartSource::ChartSource(wxString name, wxString url, wxString localdir)
{
      m_name = name;
      m_url = url;
      m_dir = localdir;
}

#define ID_MNU_SELALL 2001
#define ID_MNU_DELALL 2002
#define ID_MNU_INVSEL 2003
#define ID_MNU_SELUPD 2004
#define ID_MNU_SELNEW 2005

void ChartDldrPanelImpl::OnPopupClick(wxCommandEvent &evt)
{
	switch(evt.GetId()) {
		case ID_MNU_SELALL:
                  m_clCharts->CheckAll(true);
			break;
		case ID_MNU_DELALL:
                  m_clCharts->CheckAll(false);
			break;
            case ID_MNU_INVSEL:
                  for (int i = 0; i < m_clCharts->GetItemCount(); i++)
                        m_clCharts->Check(i, !m_clCharts->IsChecked(i));
			break;
            case ID_MNU_SELUPD:
            {
                  ChartSource *cs = pPlugIn->m_chartSources->Item(GetSelectedCatalog());
                  FillFromFile(cs->GetUrl(), cs->GetDir(), false, true);
                  break;
            }
            case ID_MNU_SELNEW:
            {
                  ChartSource *cs = pPlugIn->m_chartSources->Item(GetSelectedCatalog());
                  FillFromFile(cs->GetUrl(), cs->GetDir(), true, false);
                  break;
            }
	}
}

void ChartDldrPanelImpl::OnContextMenu( wxMouseEvent& event )
{
      if (m_clCharts->GetItemCount() == 0)
            return;
      wxMenu menu;
      wxPoint point = event.GetPosition();
      wxPoint p1 = ((wxWindow *)m_clCharts)->GetPosition();

      // add stuff
      menu.Append(ID_MNU_SELALL, _("Select all"), wxT(""));
      menu.Append(ID_MNU_DELALL, _("Deselect all"), wxT(""));
      menu.Append(ID_MNU_INVSEL, _("Invert selection"), wxT(""));
      menu.Append(ID_MNU_SELUPD, _("Select updated"), wxT(""));
      menu.Append(ID_MNU_SELNEW, _("Select new"), wxT(""));
      menu.Connect(wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&ChartDldrPanelImpl::OnPopupClick, NULL, this);
      // and then display
      PopupMenu(&menu, p1.x + point.x, p1.y + point.y);
}

void ChartDldrPanelImpl::SetSource(int id)
{
    pPlugIn->SetSourceId( id );
    
    m_bDeleteSource->Enable( id >= 0 );
    m_bUpdateChartList->Enable( id >= 0 );

    CleanForm();
    if (id >= 0 and id < pPlugIn->m_chartSources->Count())
    {
        ChartSource *cs = pPlugIn->m_chartSources->Item(id);
        cs->UpdateLocalFiles();
        pPlugIn->m_pChartSource = cs;
        FillFromFile(cs->GetUrl(), cs->GetDir());
    }
    else
    {
        pPlugIn->m_pChartSource = NULL;
    }
}

void ChartDldrPanelImpl::SelectSource( wxListEvent& event )
{
      SetSource(GetSelectedCatalog());

      event.Skip();
}

void ChartDldrPanelImpl::CleanForm()
{
//      m_tChartSourceInfo->SetValue(wxEmptyString);
      m_clCharts->DeleteAllItems();
}

void ChartDldrPanelImpl::FillFromFile(wxString url, wxString dir, bool selnew, bool selupd)
{
      //load if exists
      wxStringTokenizer tk(url, _T("/"));
      wxString file;
      do
      {
            file = tk.GetNextToken();
      } while(tk.HasMoreTokens());
      wxFileName fn;
      fn.SetFullName(file);
      fn.SetPath(dir);
      wxString path = fn.GetFullPath();
      if (wxFileExists(path))
      {
            pPlugIn->m_pChartCatalog->LoadFromFile(path);
//            m_tChartSourceInfo->SetValue(pPlugIn->m_pChartCatalog->GetDescription());
            //fill in the rest of the form
            m_clCharts->DeleteAllItems();
            for(size_t i = 0; i < pPlugIn->m_pChartCatalog->charts->Count(); i++)
            {
                  wxListItem li;
                  li.SetId(i);
                  li.SetText(pPlugIn->m_pChartCatalog->charts->Item(i).GetChartTitle());
                  long x = m_clCharts->InsertItem(li);
                  m_clCharts->SetItem(x, 0, pPlugIn->m_pChartCatalog->charts->Item(i).GetChartTitle());
                  wxString file = pPlugIn->m_pChartCatalog->charts->Item(i).GetChartFilename();
                  if (!pPlugIn->m_pChartSource->ExistsLocaly(file))
                  {
                        m_clCharts->SetItem(x, 1, _("New"));
                        if (selnew)
                              m_clCharts->Check(x, true);
                  }
                  else
                  {
                        if(pPlugIn->m_pChartSource->IsNewerThanLocal(file, pPlugIn->m_pChartCatalog->charts->Item(i).GetUpdateDatetime()))
                        {
                              m_clCharts->SetItem(x, 1, _("Update available"));
                              if (selupd)
                                    m_clCharts->Check(x, true);
                        }
                        else
                        {
                              m_clCharts->SetItem(x, 1, _("Up to date"));
                        }
                  }
                  m_clCharts->SetItem(x, 2, pPlugIn->m_pChartCatalog->charts->Item(i).GetUpdateDatetime().Format());
            }
      }
}

bool ChartSource::ExistsLocaly(wxString filename)
{
      wxStringTokenizer tk(filename, _T("."));
      wxString file = tk.GetNextToken();
      for (size_t i = 0; i < m_localfiles.Count(); i++)
      {
            wxFileName fn(m_localfiles.Item(i));
            if(fn.GetName().StartsWith(file))
                  return true;
      }
      return false;
}

bool ChartSource::IsNewerThanLocal(wxString filename, wxDateTime validDate)
{
      wxStringTokenizer tk(filename, _T("."));
      wxString file = tk.GetNextToken();
      for (size_t i = 0; i < m_localfiles.Count(); i++)
      {
            wxFileName fn(m_localfiles.Item(i));
            if(fn.GetName().StartsWith(file))
            {
                  wxDateTime ct, mt, at;
                  fn.GetTimes(&at, &mt, &ct);
                  if (validDate.IsLaterThan(ct))
                        return true;
            }
      }
      return false;
}

int ChartDldrPanelImpl::GetSelectedCatalog()
{
    long item = m_lbChartSources->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    return item;
}

void ChartDldrPanelImpl::SelectCatalog(int item)
{
    m_lbChartSources->SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
}

void ChartDldrPanelImpl::AppendCatalog(ChartSource *cs)
{
    long id = m_lbChartSources->GetItemCount();
    m_lbChartSources->InsertItem(id, cs->GetName());
    m_lbChartSources->SetItem(id, 1, _("(Please update first)"));
    wxURL url(cs->GetUrl());
    if (url.GetError() != wxURL_NOERR)
    {
        wxMessageBox(_("Error, the URL to the chart source data seems wrong."), _("Error"));
        return;
    }
    wxFileName fn(url.GetPath());
    fn.SetPath(cs->GetDir());
    wxString path = fn.GetFullPath();
    if (wxFileExists(path))
    {
        if (pPlugIn->m_pChartCatalog->LoadFromFile(path, true))
        {
            m_lbChartSources->SetItem(id, 0, pPlugIn->m_pChartCatalog->title);
            m_lbChartSources->SetItem(id, 1, pPlugIn->m_pChartCatalog->dt_valid.Format());
        }
    }
}

void ChartDldrPanelImpl::UpdateChartList( wxCommandEvent& event )
{
      //TODO: check if everything exists and we can write to the output dir etc.
      if (!m_lbChartSources->GetSelectedItemCount())
            return;
      ChartSource *cs = pPlugIn->m_chartSources->Item(GetSelectedCatalog());
      wxURL url(cs->GetUrl());
      if (url.GetError() != wxURL_NOERR)
      {
            wxMessageBox(_("Error, the URL to the chart source data seems wrong."), _("Error"));
            return;
      }

    wxStringTokenizer tk(url.GetPath(), _T("/"));
    wxString file;
    do
    {
        file = tk.GetNextToken();
    } while(tk.HasMoreTokens());
    wxFileName fn;
    fn.SetFullName(file);
    fn.SetPath(cs->GetDir());

    wxFileOutputStream output(fn.GetFullPath());
    wxCurlDownloadDialog ddlg(url.GetURL(), &output, _("Downloading file"),
        _("Reading Headers: ") + url.GetURL(), wxNullBitmap, this,
        wxCTDS_CAN_PAUSE|wxCTDS_CAN_ABORT|wxCTDS_SHOW_ALL|wxCTDS_AUTO_CLOSE);
    ddlg.SetSize(this->GetSize().GetWidth(), ddlg.GetSize().GetHeight());
    switch(ddlg.RunModal())
    {
        case wxCDRF_SUCCESS:
        {
            FillFromFile(url.GetPath(), fn.GetPath());
            long id = GetSelectedCatalog();
            m_lbChartSources->SetItem(id, 0, pPlugIn->m_pChartCatalog->title);
            m_lbChartSources->SetItem(id, 1, pPlugIn->m_pChartCatalog->dt_valid.Format());
            break;
        }
        case wxCDRF_FAILED:
        {
            wxMessageBox(wxString::Format( _("Failed to Download: %s \nVerify there is a working Internet connection."), url.GetURL().c_str() ), 
            _("Chart Downloader"), wxOK | wxICON_ERROR);
            wxRemoveFile( fn.GetFullPath() );
            break;
        }
        case wxCDRF_USER_ABORTED:
            break;
    }
}

wxArrayString ChartSource::GetLocalFiles()
{
      wxArrayString *ret = new wxArrayString();
      wxDir::GetAllFiles(GetDir(), ret);
      wxArrayString r(*ret);
      wxDELETE(ret);
      return r;
}

bool ChartDldrPanelImpl::DownloadChart(wxString url, wxString file, wxString title)
{
    if (cancelled)
        return false;
    downloading++;

    downloadInProgress = true;
    wxFileOutputStream output(file);
    wxCurlDownloadDialog ddlg(url, &output, wxString::Format(_("Downloading file %d of %d"), downloading, to_download),
        wxString::Format(_("Chart: %s"), title.c_str()), wxNullBitmap, this,
        wxCTDS_CAN_PAUSE|wxCTDS_CAN_ABORT|wxCTDS_SHOW_ALL|wxCTDS_AUTO_CLOSE);
    ddlg.SetSize(this->GetSize().GetWidth(), ddlg.GetSize().GetHeight());
    switch(ddlg.RunModal())
    {
        case wxCDRF_SUCCESS:
            return true;
        case wxCDRF_FAILED:
        {
            wxMessageBox(wxString::Format( _("Failed to Download: %s \nVerify there is a working Internet connection."), url.c_str() ), 
            _("Chart Downloader"), wxOK | wxICON_ERROR);
            wxRemoveFile( file );
            return false;
        }
        case wxCDRF_USER_ABORTED:
            wxRemoveFile( file );
            cancelled = true;
            return false;
    }
    return false;
}

void ChartDldrPanelImpl::DownloadCharts( wxCommandEvent& event )
{
      cancelled = false;
      if (!m_lbChartSources->GetSelectedItemCount())
            return;
      ChartSource *cs = pPlugIn->m_chartSources->Item(GetSelectedCatalog());
      if (m_clCharts->GetCheckedItemCount() == 0)
            return;
      to_download = m_clCharts->GetCheckedItemCount();
      downloading = 0;
      for (int i = 0; i < m_clCharts->GetItemCount(); i++)
      {
            //Prepare download queues
            if(m_clCharts->IsChecked(i))
            {
                  //download queue
                  wxURL url(pPlugIn->m_pChartCatalog->charts->Item(i).GetDownloadLocation());
                  if (url.GetError() != wxURL_NOERR)
                  {
                        wxMessageBox(_("Error, the URL to the chart data seems wrong."), _("Error"));
                        this->Enable();
                        return;
                  }
                  //construct local zipfile path
                  wxString file = pPlugIn->m_pChartCatalog->charts->Item(i).GetChartFilename();
                  wxFileName fn;
                  fn.SetFullName(file);
                  fn.SetPath(cs->GetDir());
                  wxString path = fn.GetFullPath();
                  if (wxFileExists(path))
                        wxRemoveFile(path);
                  wxString title = pPlugIn->m_pChartCatalog->charts->Item(i).GetChartTitle();

                  if( DownloadChart(url.GetURL(), path, title) )
                  {
                        wxFileName fn(path);
                        pPlugIn->ExtractZipFiles(path, fn.GetPath(), true, pPlugIn->m_pChartCatalog->charts->Item(i).GetUpdateDatetime());
                        wxRemoveFile(path);
                  }
            }
      }
      SetSource(GetSelectedCatalog());
}

ChartDldrPanelImpl::~ChartDldrPanelImpl()
{
      m_lbChartSources->ClearAll();
      ((wxListCtrl *)m_clCharts)->DeleteAllItems();
}

ChartDldrPanelImpl::ChartDldrPanelImpl( chartdldr_pi* plugin, wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style )
            : ChartDldrPanel( parent, id, pos, size, style )
{
      m_lbChartSources->InsertColumn (0, _("Catalog"), wxLIST_FORMAT_LEFT, 280);
      m_lbChartSources->InsertColumn (1, _("Released"), wxLIST_FORMAT_LEFT, 230);

      // Add columns
      ((wxListCtrl *)m_clCharts)->InsertColumn(0, _("Title"), wxLIST_FORMAT_LEFT, 300);
      ((wxListCtrl *)m_clCharts)->InsertColumn(1, _("Status"), wxLIST_FORMAT_LEFT, 100);
      ((wxListCtrl *)m_clCharts)->InsertColumn(2, _("Latest"), wxLIST_FORMAT_LEFT, 230);

      downloadInProgress = false;
      cancelled = false;
      to_download = -1;
      downloading = -1;
      pPlugIn = plugin;

      for (size_t i = 0; i < plugin->m_chartSources->GetCount(); i++)
      {
            AppendCatalog(plugin->m_chartSources->Item(i));
      }
}

void ChartDldrPanelImpl::DeleteSource( wxCommandEvent& event )
{
      if (!m_lbChartSources->GetSelectedItemCount())
            return;
      pPlugIn->m_chartSources->RemoveAt(GetSelectedCatalog());
      m_lbChartSources->DeleteItem(GetSelectedCatalog());
      SelectCatalog(-1);
      pPlugIn->SaveConfig();
      event.Skip();
}

void ChartDldrPanelImpl::AddSource( wxCommandEvent& event )
{
      ChartDldrGuiAddSourceDlg *dialog = new ChartDldrGuiAddSourceDlg(pPlugIn->m_parent_window);
      if(dialog->ShowModal() == wxID_OK)
      {
            ChartSource *cs = new ChartSource(dialog->m_tSourceName->GetValue(), dialog->m_tChartSourceUrl->GetValue(), dialog->m_dpChartDirectory->GetPath());
            pPlugIn->m_chartSources->Add(cs);
            AppendCatalog(cs);
            /*m_lbChartSources->Select(m_lbChartSources->GetChildren().GetCount() - 1);
            m_tChartSourceUrl->SetValue(dialog->m_tChartSourceUrl->GetValue());
            m_dpChartDirectory->SetPath(dialog->m_dpChartDirectory->GetPath());*/

            pPlugIn->SaveConfig();
      }
      dialog->Close();
      dialog->Destroy();
      wxDELETE(dialog);
      event.Skip();
}

bool chartdldr_pi::ExtractZipFiles(const wxString& aZipFile, const wxString& aTargetDir, bool aStripPath, wxDateTime aMTime) {
      bool ret = true;

      std::auto_ptr<wxZipEntry> entry(new wxZipEntry());

      do {

            wxFileInputStream in(aZipFile);

            if (!in) {
                  wxLogError(_T("Can not open file '")+aZipFile+_T("'."));
                  ret = false;
                  break;
            }
            wxZipInputStream zip(in);

            while (entry.reset(zip.GetNextEntry()), entry.get() != NULL) {
                  // access meta-data
                  wxString name = entry->GetName();
                  if (aStripPath)
                  {
                      wxFileName fn(name);
                      /* We can completly replace the entry path */
                      //fn.SetPath(aTargetDir);
                      //name = fn.GetFullPath();
                      /* Or only remove the first dir (eg. ENC_ROOT) */
                      fn.RemoveDir(0);
                      name = aTargetDir + wxFileName::GetPathSeparator() + fn.GetFullPath();
                  }
                  else
                  {
                      name = aTargetDir + wxFileName::GetPathSeparator() + name;
                  }

                  // read 'zip' to access the entry's data
                  if (entry->IsDir()) {
                        int perm = entry->GetMode();
                        wxFileName::Mkdir(name, perm, wxPATH_MKDIR_FULL);
                  } else {
                        zip.OpenEntry(*entry.get());
                        if (!zip.CanRead()) {
                              wxLogError(_T("Can not read zip entry '") + entry->GetName() + _T("'."));
                              ret = false;
                              break;
                        }

                        wxFileName fn(name);
                        if (!fn.DirExists()) {
                            wxFileName::Mkdir(fn.GetPath());
                        }

                        wxFileOutputStream file(name);

                        if (!file) {
                              wxLogError(_T("Can not create file '")+name+_T("'."));
                              ret = false;
                              break;
                        }
                        zip.Read(file);
                        wxString s = aMTime.Format();
                        fn.SetTimes(&aMTime, &aMTime, &aMTime);
                  }

            }

      } while(false);

      return ret;
}

ChartDldrGuiAddSourceDlg::ChartDldrGuiAddSourceDlg( wxWindow* parent ) : AddSourceDlg( parent )
{
      m_chartSources = new wxArrayOfChartSources();
      wxStringTokenizer st(_T(NOAA_CHART_SOURCES), _T("|"), wxTOKEN_DEFAULT);
      while ( st.HasMoreTokens() )
      {
            wxString s1 = st.GetNextToken();
            wxString s2 = st.GetNextToken();
            wxString s3 = st.GetNextToken();
            m_chartSources->Add(new ChartSource(s1, s2, s3));
      }
      m_radioBtn1->SetValue(true);

      for (size_t i = 0; i < m_chartSources->GetCount(); i++)
      {
            m_cbChartSources->Append(m_chartSources->Item(i)->GetName());
      }

}

ChartDldrGuiAddSourceDlg::~ChartDldrGuiAddSourceDlg()
{
      m_chartSources->Clear();
      wxDELETE(m_chartSources);
}

void ChartDldrGuiAddSourceDlg::OnChangeType( wxCommandEvent& event )
{
      m_cbChartSources->Enable(m_radioBtn1->GetValue());
      m_tSourceName->Enable(m_radioBtn2->GetValue());
      m_tChartSourceUrl->Enable(m_radioBtn2->GetValue());
}

void ChartDldrGuiAddSourceDlg::OnSourceSelected( wxCommandEvent& event )
{
      ChartSource *cs = m_chartSources->Item(m_cbChartSources->GetSelection());
      m_tSourceName->SetValue(cs->GetName());
      m_tChartSourceUrl->SetValue(cs->GetUrl());

      event.Skip();
}

