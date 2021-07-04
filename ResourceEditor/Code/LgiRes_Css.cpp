#include "lgi/common/Lgi.h"
#include "LgiResEdit.h"
#include "lgi/common/TableLayout.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Edit.h"
#include "lgi/common/TextView3.h"

/////////////////////////////////////////////////////////////////////////////
enum Ids {
    IDC_STATIC = -1,
    IDC_NAME = 100,
    IDC_STYLE
};
class ResCssUi : public LTableLayout
{
    ResCss *Css;
    LEdit *Name;
    GTextView3 *Style;
    
public:
    ResCssUi(ResCss *css)
    {
        Css = css;
        int y = 0;
        GLayoutCell *c;
        
        if ((c = GetCell(0, y++)))
            c->Add(new LTextLabel(IDC_STATIC, 0, 0, -1, -1, "Name:"));

        if ((c = GetCell(0, y++)))
        {
            c->Add(Name = new LEdit(IDC_NAME, 0, 0, 80, 20, 0));
            Name->Name(Css->Name());
        }

        if ((c = GetCell(0, y++)))
            c->Add(new LTextLabel(IDC_STATIC, 0, 0, -1, -1, "Style:"));

        if ((c = GetCell(0, y++)))
        {
            c->Add(Style = new GTextView3(IDC_STYLE, 0, 0, 80, 20, 0));
            Style->Name(Css->Style);
            Style->Sunken(true);
        }
        
        InvalidateLayout();
    }
    
    ~ResCssUi()
    {
        Save();
        Css->Ui = NULL;
    }
    
    void Save()
    {
        Css->Name(Name->Name());
        Css->Style.Reset(NewStr(Style->Name()));
    }
};

/////////////////////////////////////////////////////////////////////////////
ResCss::ResCss(AppWnd *w, int type) : Resource(w, type)
{
    Name("Css Style");
    Ui = 0;
}

ResCss::~ResCss()
{
}

void ResCss::Create(LXmlTag *load, SerialiseContext *Ctx)
{
    if (load && Ctx)
    {
        Read(load, *Ctx);
    }
}

void ResCss::OnShowLanguages()
{
}

LView *ResCss::CreateUI()
{
    return Ui = new ResCssUi(this);
}

void ResCss::OnRightClick(LSubMenu *RClick)
{
}

void ResCss::OnCommand(int Cmd)
{
}

int ResCss::OnCommand(int Cmd, int Event, OsView hWnd)
{
    return false;
}

bool ResCss::Test(ErrorCollection *e)
{
    return true;
}

bool ResCss::Read(LXmlTag *t, SerialiseContext &Ctx)
{
    if (!t->IsTag("style"))
        return false;
    
    Name(t->GetAttr("name"));
    Style.Reset(NewStr(t->GetContent()));
    
    return true;
}

bool ResCss::Write(LXmlTag *t, SerialiseContext &Ctx)
{
    if (Ui)
        Ui->Save();
    
    t->SetTag("style");
    t->SetAttr("name", Name());
    t->SetContent(Style);
    return true;
}
