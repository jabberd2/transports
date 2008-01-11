use strict;
use LWP::UserAgent;
use HTTP::Request::Common qw(GET POST);
use HTTP::Response;
use HTTP::Cookies; 
use Unicode::Lite;

my $PLUGIN_NAME			= "EraOmnix_Podstawowa";
my $PLUGIN_VERSION		= "0.4-20050930-kg";
my $DEFAULT_PRIO		= 4;
my $DEST_NUMBERS_REGEXP	= '^\+48(88[80]|60[24680]|69[24680]|66[02468])'; # needed linke this in // good numberz?

use vars qw ( %EraOmnix_Podst_accounts );

# RULES:
# 1. ALWAYS, ALWAYS, ALWAYS create new presence/message/iq
# 2. Create handlers, plugin is named ser.vi.ce/Plugin
# 3. Provide ALL info into %plugin_data

RegisterEvent('iq','^'.$config{service_name}."/".$PLUGIN_NAME.'$',\&EraOmnix_Podst_InIQ); # for registration
RegisterEvent('pre_connect','.+',\&EraOmnix_Podst_PreConn); # for loading some data ;)

$plugin_data{$PLUGIN_NAME}->{inmessage_h}=\&EraOmnix_Podst_InMessage;
$plugin_data{$PLUGIN_NAME}->{will_take_h}=\&EraOmnix_Podst_WillTake;
$plugin_data{$PLUGIN_NAME}->{default_prio}=$DEFAULT_PRIO;
$plugin_data{$PLUGIN_NAME}->{version}=$PLUGIN_VERSION;
$plugin_data{$PLUGIN_NAME}->{needs_reg}=1;

sub EraOmnix_Podst_WillTake {
        my $from=shift;
        my $to_nr=shift; # if not provided, it means it asks for the service, w/o number served by it
        return (($to_nr?($to_nr =~ /$DEST_NUMBERS_REGEXP/):1)&&(exists($EraOmnix_Podst_accounts{$from})));
};

sub EraOmnix_Podst_PreConn {
#	%EraOmnix_Podst_accounts=%{LoadXMLHash('accounts',$config{plugins}->{$PLUGIN_NAME}->{accounts_file},$PLUGIN_NAME)};
	my $t=tie(%EraOmnix_Podst_accounts,"MLDBM::Sync",$config{plugins}->{$PLUGIN_NAME}->{accounts_file},O_CREAT|O_RDWR,0600);
	log2($PLUGIN_NAME.": Ready.");
};

sub EraOmnix_Podst_InIQ {
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
		my $account=$EraOmnix_Podst_accounts{$base_from};
		if ($type eq 'get') {
			$iq->SetType('result');
			$query->SetInstructions('Send a message to: +48TELNUMBER@'.$config{service_name}." to send an SMS.\n".
				"You can add a contact like this to your roster.\n\n".
				"Provide your Username (48xxxxxxxxx) and Password to ".$PLUGIN_NAME." gateway.\n".
				"See http://www.eraomnix.pl/sms for info.\n\n".                
				"Note: x:data compilant client allows setting more options.");
			$query->SetUsername( ($account?$account->{username}:'') );
			$query->SetPassword('');
			# x:data ...
			my $xd=( (Net::Jabber->VERSION>1.30) ? new Net::Jabber::Stanza("x") : new Net::Jabber::X );
			$xd->SetXMLNS('jabber:x:data');
			$xd->SetData(instructions=>'Send a message to: +48TELNUMBER@'.$config{service_name}." to send an SMS.\n".
					"You can add a contact like this to your roster.\n".
					"Provide your Username and Password to ".$PLUGIN_NAME." gateway.\n".
					"See http://www.eraomnix.pl/sms for info.\n".
					"You can configure additional options too. When changing options, remember to provide ".
					"your password!",
				title=>'EraOmnix_Podstawowa Registration',
				type=>'form');
			$xd->AddField(type=>'text-single',var=>'username',label=>'User name',
				value=>($account?$account->{username}:''));
			$xd->AddField(type=>'text-private',var=>'password',label=>'Password (!) ');
			$xd->AddField(type=>'text-single',var=>'contact',label=>'Return contact',
				value=>($account?$account->{contact}:''));
			$xd->AddField(type=>'text-single',var=>'signature',label=>'Signature',
				value=>($account?$account->{signature}:''));
			$query->AddX($xd);
			$Connection->Send($iq);
		} elsif ($type eq 'set') {
			# x:data ...
			my @xd=$query_o->GetX('jabber:x:data'); my %f;
			if ($#xd>-1) {
				foreach my $x ($xd[0]->GetFields()) { $f{$x->GetVar()}=$x->GetValue(); };
			} else {
				$f{username}=$query_o->GetUsername(); $f{password}=$query_o->GetPassword();
				$f{contact}=''; $f{signature}=$base_from; $f{signature}=~s/@.+$//g;
			};
			# ... x:data
			if (($f{username} eq '')&&($f{password} eq '')) {
				SendPluginPresencesToUser($PLUGIN_NAME,'unavailable',$from);
				delete $EraOmnix_Podst_accounts{$base_from};
			} else {
				for my $i (qw(username password contact signature)) { $account->{$i}=$f{$i}; };
				$EraOmnix_Podst_accounts{$base_from}=$account;
                SendPluginPresencesToUser($PLUGIN_NAME,'available',$from);
				PushAgentToUsersRoster($from); # important in any plugin which needs registration!
			};
#			SaveXMLHash(\%EraOmnix_Podst_accounts,'accounts',$config{plugins}->{$PLUGIN_NAME}->{accounts_file},$PLUGIN_NAME);
			$iq->SetType('result');
			$Connection->Send($iq);
		};
	};
};

my %EraOmnix_Podst_errmsgs=(
	'0'=>'wysy³ka bez b³êdu',
	'1'=>'awaria systemu',
	'2'=>'u¿ytkownik nieautoryzowany',
	'3'=>'dostêp zablokowany',
	'5'=>'b³±d sk³adni',
	'7'=>'wyczerpany limit SMS',
	'8'=>'b³êdny adres odbiorcy SMS',
	'9'=>'wiadomo¶æ zbyt d³uga',
	'10'=>'brak wymaganej liczby ¿etonów'
);

sub EraOmnix_Podst_InMessage {
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

	my $result_message="";
	$nr=~s/\+//g;
	my $sig=$base_from; $sig=~s/@.+$//g;

	my $account=$EraOmnix_Podst_accounts{$base_from};
    
    $result_message=sendSmsViaOmnix($account->{username}, $account->{password}, $nr, convert('utf8','latin2',$message_o->GetBody()), $sig);
	$message->SetBody("www.eraomnix.pl: ".convert('latin2','utf8', $result_message));
	$Connection->Send($message);
};

# credits:
# Jacek Fiok <jfiok@jfiok.org>; http://sms.jfiok.org 
# Piotr W³odarczyk <piotr@wlodarczyk.waw.pl> 
sub sendSmsViaOmnix
{
	my ($login, $password, $number, $message, $sig) = @_;    

    my $cookie_jar = HTTP::Cookies->new; 
    my $ua = new LWP::UserAgent;
    $ua->timeout(30);
    $ua->agent("Mozilla/3.0 (X11, I, Linux 2.4.0 i486");
    $ua->env_proxy();
    $ua->cookie_jar($cookie_jar); 
    push @{ $ua->requests_redirectable }, 'POST';
    my $token;

    # 1. get sms-index
    my $res = $ua->request (GET 'http://www.eraomnix.pl/msg/user/sponsored/welcome.do');
    return "B³±d przy otwieraniu formularza [1]" unless $res->is_success;
    if ($res->content =~ /TOKEN\" value=\"(.*)\"/ ) { $token=$1;} 

    # 2. send the POST login form
    my $req = POST "http://www.eraomnix.pl/sso2/authenticate.do", [
    	login => $login,
        password => $password,
        "org.apache.struts.taglib.html.TOKEN" => $token
    ];

    $res = $ua->request($req);
    return "B³±d przy logowaniu - zerwane po³±czenie [2]" unless $res->is_success;
    return "B³±d przy logowaniu - nieprawid³owe has³o [2]" unless $res->content =~ /Pozosta³o SMS/;

    my $poczatek=0;
    my $dlugosc=110;

    my $wiadomosc=$message;
    my $ilosc_smsow = 1; 
    
    if ((length($wiadomosc) % $dlugosc) != 0) { 
        $ilosc_smsow = ((length($wiadomosc) - (length($wiadomosc) % $dlugosc)) / $dlugosc) + 1; 
    } else {
        $ilosc_smsow = length($wiadomosc) / $dlugosc;
    }

    # pêtla wysy³aj±ca po kawa³ku
    for (my $i=1;$i<=$ilosc_smsow;$i++) 
    {
        $message=substr($wiadomosc,$poczatek,$dlugosc);
        $poczatek += $dlugosc;

        my $sms_zostalo = 666;
        if ($res->content =~ /Pozosta.o SMS.w: \<b\>([0-9]+)\<\/b\>/) { $sms_zostalo = $1; }
        return "Nie mogê odczytaæ ilo¶ci dostêpnych SMSów" if $sms_zostalo == 666;
        return "Limit wiadomo¶ci na dzi¶ przekroczony" if $sms_zostalo == 0;
    	
        # 3. wyslij
        if ($res->content =~ /TOKEN\" value=\"(.*)\"/ ) { $token=$1;} else { return "Nie widzê tokena!"; }
        
        $req = POST 'http://www.eraomnix.pl/msg/user/sponsored/sms.do', [
        	"top.phoneReceiver" => $number,
            "top.text"  => $message,
            "org.apache.struts.taglib.html.TOKEN" => $token,
            "top.signature"	=> $sig,
            "top.characterLimit" => '99',
            "send" => '0',
            "mmsTab" => 'mmsTab',
            "send_x" => '15',
            "send_y" => '13'
        ];

        $res = $ua->request($req);
        if (!($res->is_success)) { return "B³±d przy podgl±dzie wiadomo¶ci [3]"; }

        # 4. czy sie wys³a³o (?).
        my $sms_zostalo2 = 666;
        if ($res->content =~ /Pozosta³o SMSów: \<b\>([0-9]+)\<\/b\>/) { $sms_zostalo2 = $1; }
        if ($sms_zostalo2 == 666) { zakoncz ("Nie mogê odczytaæ ilo¶ci dostêpnych SMSów", 1); }
        if ($sms_zostalo > $sms_zostalo2) 
        {
            return "Wszystko OK; pozosta³y limit wiadomo¶ci: ".$sms_zostalo2; 
        } else {
            return "Wiadomo¶æ wys³ana, ale STATUS NIEZNANY (pozosta³y limit: ".$sms_zostalo2.").";
        }
    }    
}

1;
