use strict;
use LWP::UserAgent;
use HTTP::Request::Common qw(GET POST);
use HTTP::Response;
use Unicode::Lite;
use HTTP::Cookies;


my $PLUGIN_NAME			= "OrangeMbox";
my $PLUGIN_VERSION		= "0.5-20070501-kg";
my $DEFAULT_PRIO		= 3;
my $DEST_NUMBERS_REGEXP		=  '^\+[0-9]+'; # needed linke this in //, BUT it's not for everyone (only ideowcy maj\xc4\x85 dowsz\xc4\x85d)

use vars qw ( %OrangeMbox_accounts );

# RULES:
# 1. ALWAYS, ALWAYS, ALWAYS create new presence/message/iq
# 2. Create handlers, plugin is named ser.vi.ce/Plugin
# 3. Provide ALL info into %plugin_data

RegisterEvent('iq','^'.$config{service_name}."/".$PLUGIN_NAME.'$',\&OrangeMbox_InIQ); # for registration
RegisterEvent('pre_connect','.+',\&OrangeMbox_PreConn); # for loading some data ;)

$plugin_data{$PLUGIN_NAME}->{inmessage_h}=\&OrangeMbox_InMessage;
$plugin_data{$PLUGIN_NAME}->{will_take_h}=\&OrangeMbox_WillTake;
$plugin_data{$PLUGIN_NAME}->{default_prio}=$DEFAULT_PRIO;
$plugin_data{$PLUGIN_NAME}->{version}=$PLUGIN_VERSION;
$plugin_data{$PLUGIN_NAME}->{needs_reg}=1;

sub OrangeMbox_WillTake {
        my $from=shift;
        my $to_nr=shift; # if not provided, it means it asks for the service, w/o number served by it 
        return (($to_nr?($to_nr =~ /$DEST_NUMBERS_REGEXP/):1)&&(exists($OrangeMbox_accounts{$from})));
};

sub OrangeMbox_PreConn {
#	%OrangeMbox_accounts=%{LoadXMLHash('accounts',$config{plugins}->{$PLUGIN_NAME}->{accounts_file},$PLUGIN_NAME)};
	my $t=tie(%OrangeMbox_accounts,"MLDBM::Sync",$config{plugins}->{$PLUGIN_NAME}->{accounts_file},O_CREAT|O_RDWR,0600);
#	$t->SyncCacheSize($config{numbers_tiehash_cache}) TODO eventually
	log2($PLUGIN_NAME.": Ready.");
};

sub OrangeMbox_InIQ {
	my $iq_o=shift;
	my $xmlns=shift;
	my $query_o=$iq_o->GetQuery();
	my $type=$iq_o->GetType();
	my $from=$iq_o->GetFrom(); my $to=$iq_o->GetTo();
	my $iq=new Net::Jabber::IQ;
	$iq->SetFrom($to); $iq->SetTo($from); $iq->SetID($iq_o->GetID);
	my $query=$iq->NewQuery($xmlns);

	if ($xmlns eq 'jabber:iq:register') {
		my $base_from=$from; $base_from=~s/\/.+$//g if ($base_from =~ /\//);
		if ($type eq 'get') {
			$iq->SetType('result');
			$query->SetInstructions('Send a message to: +48TELNUMBER@'.$config{service_name}." to send an SMS.\n".
				"You can add a contact like this to your roster.\n\n".
				"Provide your Username and Password to ".$PLUGIN_NAME." gateway.\n".
				"See http://www.orange.pl/ for info.");
			$query->SetUsername( ($OrangeMbox_accounts{$base_from}?$OrangeMbox_accounts{$base_from}->{username}:'') );
			$query->SetPassword('');
			$Connection->Send($iq);
		} elsif ($type eq 'set') {
			if (($query_o->GetUsername() eq '')&&($query_o->GetPassword() eq '')) {
				SendPluginPresencesToUser($PLUGIN_NAME,'unavailable',$from);
				delete $OrangeMbox_accounts{$base_from};
			} else {
				my $a=$OrangeMbox_accounts{$base_from};
				$a->{username}=$query_o->GetUsername();
				$a->{password}=$query_o->GetPassword();
				$OrangeMbox_accounts{$base_from}=$a;
				SendPluginPresencesToUser($PLUGIN_NAME,'available',$from);
				PushAgentToUsersRoster($from); # important in any plugin which needs registration!
			};
#			SaveXMLHash(\%OrangeMbox_accounts,'accounts',$config{plugins}->{$PLUGIN_NAME}->{accounts_file},$PLUGIN_NAME);
			$iq->SetType('result');
			$Connection->Send($iq);
		};
	};
};

sub OrangeMbox_InMessage {
	my $message_o=shift;
	my $type=shift;
	my $to=$message_o->GetTo(); my $from=$message_o->GetFrom();
	my $message=new Net::Jabber::Message;
	$message->SetTo($from); $message->SetFrom($to); $message->SetType($type);
	my $SN=$config{service_name};
	my $PN=$SN."/".$PLUGIN_NAME;
	my $base_from=$from; $base_from=~s/\/.+$//g if ($base_from =~ /\//);
	my $nr=$to; $nr=~s/@.+$//g;
	return unless ($to =~ /$PN$/); # shouldn't happen

	# the guts:
    
    my $account = $OrangeMbox_accounts{$base_from};
    my $result_message=sendSmsViaOrange($account->{username}, $account->{password}, $nr, $message_o->GetBody());
	$message->SetBody("www.orange.pl: ".$result_message);
	$Connection->Send($message);
};

# credits:
# Jacek Fiok <jfiok@jfiok.org>; http://sms.jfiok.org
# Spley <spley@home.pl>
# Maciej Krzyzanowski <spider@popnet.pl>
# Rafal 'RaV.' Matczak <rafal.matczak.orangutan.poczta.finemedia.pl>
sub sendSmsViaOrange
{
    my ($login, $password, $number, $message) = @_;  

    my $cookie_jar = HTTP::Cookies->new;
    my $ua = new LWP::UserAgent;
    $ua->timeout(30);
    $ua->agent("Lynx/2.8.5rel.1 libwww-FM/2.14 SSL-MM/1.4.1 OpenSSL/0.9.7d");
    $ua->no_proxy('www.orange.pl');
    $ua->cookie_jar($cookie_jar);

    my $res; my $req;

    $number =~ s/^\+48//;
    $number =~ s/^00//;

    push @{ $ua->requests_redirectable }, 'POST';


    # 1. get sms-index
    $res = $ua->request (GET 'http://www.orange.pl/portal/map/map/signin');
    return "B\xc5\x82\xc4\x85d przy otwieraniu formularza [1]" unless $res->is_success;

    # 2. send the POST login form
    # FIXME ssl!


	$req = POST 'http://www.orange.pl/portal/map/map/signin?_DARGS=/gear/static/home/login.jsp.loginFormId', [
	'_dyncharset'=>'UTF-8',
	'/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.loginErrorURL'=>'http://www.orange.pl/portal/map/map/signin',
	'_D:/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.loginErrorURL'=>' ',
	'/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.loginSuccessURL'=>'http://www.orange.pl/portal/map/map/pim',
	'_D:/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.loginSuccessURL'=>' ',
	'/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.value.login'=>$login,
	'_D:/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.value.login'=>' ',
	'/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.value.password'=>$password,
	'_D:/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.value.password'=>' ',
	'/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.login.x'=>0,
	'/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.login.y'=>0,
	'/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.login'=>' ',
	'_D:/amg/ptk/map/core/formhandlers/AdvancedProfileFormHandler.login'=>' ',
	'_DARGS' => '/gear/static/signIn.jsp',
];

    $req->referer ('http://www.orange.pl/portal/map/map/signin');
    $res = $ua->request($req);
    return "B\xc5\x82\xc4\x85d przy logowaniu [2]" unless $res->is_success;
    return "B\xc5\x82\xc4\x85d przy logowaniu - nieprawid\xc5\x82owe has\xc5\x82o? [2]" unless $res->content =~ /witaj/;

    
    $req = GET 'http://www.orange.pl/portal/map/map/message_box?mbox_view=newsms&mbox_edit=new';
    $res = $ua->request($req);

    # Nie do konca odczytuje jeszcze ilosc pozostalych smsow (jesli sa jeszcze z doladowan)
    # FIXME zrobic to ladniej..
    my $sms_zostalo = 666;
    my $cnt = $res->content;

    if ($res->content =~ /<span class=\"label\">bezp..atne :<\/span>\s*<span class=\"value\">([0-9]+)<\/span>/) { $sms_zostalo = eval ($1); }

    return "Nie mog\xc4\x99 odczyta\xc4\x87 ilo\xc5\x9bci dost\xc4\x99pnych SMS\xc3\xb3w" if $sms_zostalo == 666;
    return "Limit wiadomo\xc5\x9bci na ten miesi\xc4\x85c przekroczony" if $sms_zostalo == 0;



    $res = $ua->request (GET 'http://www.orange.pl/portal/map/map/message_box?mbox_view=newsms&mbox_edit=new');
    return "B\xc5\x82\xc4\x85d przy otwarciu formularza SMS [4]" unless $res->is_success;

    # _DARGS=/gear/mapmessagebox/smsform.jsp na WWW jest i w GET string i w POST :)
    $req = POST 'http://www.orange.pl/portal/map/map/message_box?_DARGS=/gear/mapmessagebox/smsform.jsp', [
    '_dyncharset' => 'UTF-8',
	'/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.type' => 'sms',
	'_D:/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.type' => ' ',
	'/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.errorURL' => '/portal/map/map/message_box?mbox_view=newsms',
	'_D:/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.errorURL'  => ' ',
	'/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.successURL' => '/portal/map/map/message_box?mbox_view=messageslist',
	'_D:/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.successURL' => ' ',
	'/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.to' => $number,
	'_D:/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.to' => ' ',
	'_D:/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.body' => ' ',
	'/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.body' => $message,
	'counter' => (640 - length($message)),
	'/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.create.x' => 0,
	'/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.create.y' => 0,
	'_D:/amg/ptk/map/messagebox/formhandlers/MessageFormHandler.create' => ' ',
	'_DARGS' => '/gear/mapmessagebox/smsform.jsp'
	];

    $req->referer('http://www.orange.pl/portal/map/map/message_box?mbox_view=newsms&mbox_edit=new');
    $res = $ua->request($req);

    return "B\xc5\x82\xc4\x85d przy ostatecznym wysy\xc5\x82aniu SMS [5]" unless $res->is_success; 
    my $sms_zostalo2 = 666;
    $cnt  = $res->content;

    if ($cnt =~ /<span class=\"label\">bezp..atne :<\/span>\n.*<span class=\"value\">([0-9]+)<\/span>/) { $sms_zostalo2 = eval ($1); }
   
    return "Nie mog\xc4\x99 odczyta\xc4\x87 ilo\xc5\x9bci dost\xc4\x99pnych SMS\xc3\xb3w" if $sms_zostalo2 == 666;
   if ($cnt =~ /Nie mo..esz wys..a.. SMS..w poza sie.. Orange/) { return "Nieprawid\xc5\x82owy numer - bramka wysy\xc5\x82a SMSy tylko do Orange (pozosta\xc5\x82y limit: ".$sms_zostalo2.")", 2; }

    if ($sms_zostalo > $sms_zostalo2) {
        return "Wszystko OK; pozosta\xc5\x82y limit wiadomo\xc5\x9bci: ".$sms_zostalo2; 
    } else {
        return "Wiadomo\xc5\x9b\xc4\x87 wys\xc5\x82ana, ale STATUS NIEZNANY (pozosta\xc5\x82y limit: ".$sms_zostalo2.").";
    }
}

1;

