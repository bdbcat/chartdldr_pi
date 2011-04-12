/******************************************************************************
 * $Id: chartcatalog.h,v 1.0 2011/02/26 01:54:37 nohal Exp $
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

#ifndef _CHARTCATALOG_H_
#define _CHARTCATALOG_H_

#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include "../../../include/tinyxml.h"

//Forward declarations
class NoticeToMariners;
class Vertex;
class Panel;
class Chart;
//WX_DECLARE_OBJARRAY(NoticeToMariners *, wxArrayOfNoticeToMariners);
WX_DECLARE_OBJARRAY(Vertex, wxArrayOfVertexes);
WX_DECLARE_OBJARRAY(Panel, wxArrayOfPanels);
WX_DECLARE_OBJARRAY(Chart, wxArrayOfCharts);

//Declarations
class ChartCatalog
{
public:
      ChartCatalog();
      ~ChartCatalog();
      // public methods
      bool LoadFromFile(wxString path);
      bool LoadFromXml(TiXmlDocument * doc);
      wxString GetDescription();

      // public properties
      wxString title;
      wxDateTime date_created;
      wxDateTime time_created;
      wxDateTime date_valid;
      wxDateTime time_valid;
      wxDateTime dt_valid;
      wxString ref_spec;
      wxString ref_spec_vers;
      wxString s62AgencyCode;
      wxArrayOfCharts *charts;
private:
      bool ParseNoaaHeader(TiXmlElement * xmldata);
};

class Chart
{
public:
      Chart(TiXmlNode * xmldata);
      ~Chart();
      // public methods

      // public properties
      wxString title; //RNC: <title>, ENC:<lname>
      wxArrayString *coast_guard_districts;
      wxArrayString *states;
      wxArrayString *regions;
      wxString zipfile_location;
      wxDateTime zipfile_datetime;
      wxDateTime zipfile_datetime_iso8601;
      int zipfile_size;

      NoticeToMariners *nm;
      NoticeToMariners *lnm;
      wxArrayOfPanels *coverage;
};

class RasterChart : public Chart //<chart>
{
public:
      RasterChart(TiXmlNode * xmldata);
      // public methods

      //public properties
      wxString number;
      int source_edition;
      int raster_edition;
      int ntm_edition;
      wxString source_date;
      wxString ntm_date;
      wxString source_edition_last_correction;
      wxString raster_edition_last_correction;
      wxString ntm_edition_last_correction;
};

class EncCell : public Chart //<cell>
{
public:
      EncCell(TiXmlNode * xmldata);
      // public methods

      //public properties
      wxString name;
      wxString src_chart;
      int cscale;
      wxString status;
      int edtn;
      int updn;
      wxDateTime uadt;
      wxDateTime isdt;
};

class NoticeToMariners //for <nm> and <lnm>
{
public:
      NoticeToMariners(TiXmlNode * xmldata);
      // public methods

      //public properties
      wxString agency; //<nm_agency> or <lnm_agency>
      wxString doc;
      wxDateTime date;
};

class Vertex
{
public:
      Vertex(TiXmlNode * xmldata);
      // public methods

      //public properties
      double lat;
      double lon;
};

class Panel
{
public:
      Panel(TiXmlNode * xmldata);
      ~Panel();
      // public methods

      //public properties
      int panel_no;
      wxArrayOfVertexes *vertexes;
};

class RncPanel : public Panel
{
public:
      RncPanel(TiXmlNode * xmldata);
      // public methods

      //public properties
      wxString panel_title;
      wxString file_name;
      int scale;
};

class EncPanel : public Panel
{
public:
      EncPanel(TiXmlNode * xmldata);
      // public methods

      //public properties
      wxString type;
};

#endif //_CHARTCATALOG_H_