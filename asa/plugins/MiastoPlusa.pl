use strict;
use LWP::UserAgent;
use HTTP::Request::Common qw(GET POST);
use HTTP::Response;
use Unicode::Lite;

my $PLUGIN_NAME			= "MiastoPlusa";
my $PLUGIN_VERSION		= "0.8-20071113";
my $DEFAULT_PRIO		= 2;
my $DEST_NUMBERS_REGEXP		=  '^\+[0-9]+';

use vars qw ( %MiastoPlusa_accounts );

# RULES:
# 1. ALWAYS, ALWAYS, ALWAYS create new presence/message/iq
# 2. Create handlers, plugin is named ser.vi.ce/Plugin
# 3. Provide ALL info into %plugin_data

RegisterEvent('iq','^'.$config{service_name}."/".$PLUGIN_NAME.'$',\&MiastoPlusa_InIQ); # for registration
RegisterEvent('pre_connect','.+',\&MiastoPlusa_PreConn); # for loading some data ;)

$plugin_data{$PLUGIN_NAME}->{inmessage_h}=\&MiastoPlusa_InMessage;
$plugin_data{$PLUGIN_NAME}->{will_take_h}=\&MiastoPlusa_WillTake;
$plugin_data{$PLUGIN_NAME}->{default_prio}=$DEFAULT_PRIO;
$plugin_data{$PLUGIN_NAME}->{version}=$PLUGIN_VERSION;
$plugin_data{$PLUGIN_NAME}->{needs_reg}=1;

sub MiastoPlusa_WillTake {
	my $from=shift;
	my $to_nr=shift; # if not provided, it means it asks for the service, w/o number served by it 
	return (($to_nr?($to_nr =~ /$DEST_NUMBERS_REGEXP/):1)&&(exists($MiastoPlusa_accounts{$from})));
};

sub MiastoPlusa_PreConn {
#	%MiastoPlusa_accounts=%{LoadXMLHash('accounts',$config{plugins}->{$PLUGIN_NAME}->{accounts_file},$PLUGIN_NAME)};
	my $t=tie(%MiastoPlusa_accounts,"MLDBM::Sync",$config{plugins}->{$PLUGIN_NAME}->{accounts_file},O_CREAT|O_RDWR,0600);
	log2($PLUGIN_NAME.": Ready.");
};

sub MiastoPlusa_InIQ {
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
		my $account=$MiastoPlusa_accounts{$base_from};
		if ($type eq 'get') {
			$iq->SetType('result');
			$query->SetInstructions('Send a message to: +48TELNUMBER@'.$config{service_name}." to send an SMS.\n".
				"You can add a contact like this to your roster.\n".
				"Provide your Username and Password to ".$PLUGIN_NAME." gateway.\n".
				"See http://www.miastoplusa.pl/ for info.\n\n".
				"Note: x:data compilant client allows setting more options.");
			$query->SetUsername(($account?$account->{username}:''));
			$query->SetPassword('');
			# x:data ...
			my $xd;
			if (Net::Jabber->VERSION > 1.30) {
				$xd=new Net::Jabber::Stanza("x");
			} else {
				$xd=new Net::Jabber::X;
			};
			$xd->SetXMLNS('jabber:x:data');
			$xd->SetData(instructions=>'Send a message to: +48TELNUMBER@'.$config{service_name}." to send an SMS.\n".
					"You can add a contact like this to your roster.\n".
					"Provide your Username and Password to ".$PLUGIN_NAME." gateway.\n".
					"See http://www.miastoplusa.pl/ for info.\n".
					"You can configure additional options too. When changing options, remember to provide ".
					"your password!",
				title=>'MiastoPlusa Registration',
				type=>'form');
			$xd->AddField(type=>'text-single',var=>'username',label=>'User name',
				value=>($account?$account->{username}:''));
			$xd->AddField(type=>'text-private',var=>'password',label=>'Password (!) ');
			my $xd_ack=$xd->AddField(type=>'list-single',var=>'ack',label=>'SMS Delivery Report to',
				value=>($account->{ack}?$account->{ack}:'phone'));
			$xd_ack->AddOption(label=>'none',value=>'none');
			$xd_ack->AddOption(label=>'phone',value=>'phone');
			$xd_ack->AddOption(label=>'web',value=>'web');
			my $xd_validity=$xd->AddField(type=>'list-single',var=>'validity',label=>'SMS Vaild for',
				value=>($account->{validity}?$account->{validity}:'24'));
			$xd_validity->AddOption(label=>'8 hours',value=>'8');
			$xd_validity->AddOption(label=>'24 hours',value=>'24');
			$xd_validity->AddOption(label=>'48 hours',value=>'48');
			$xd_validity->AddOption(label=>'72 hours',value=>'72');
			my $xd_arch=$xd->AddField(type=>'boolean',var=>'arch',label=>'Archive sent SMS',
				value=>($account->{arch}?$account->{arch}:0));
#			$xd_arch->AddOption(value=>'no'); $xd_arch->AddOption(value=>'yes');
			my $xd_flash=$xd->AddField(type=>'boolean',var=>'flash',label=>'Send flash SMS',
				value=>($account->{flash}?$account->{flash}:0));
#			$xd_flash->AddOption(value=>'no'); $xd_flash->AddOption(value=>'yes');
			$query->AddX($xd);
			# ... x:data
			$Connection->Send($iq);
		} elsif ($type eq 'set') {
			# x:data ...
			my @xd=$query_o->GetX('jabber:x:data'); my %f;
			if ($#xd>-1) {
				foreach my $x ($xd[0]->GetFields()) { $f{$x->GetVar()}=$x->GetValue(); };
			} else {
				$f{username}=$query_o->GetUsername(); $f{password}=$query_o->GetPassword();
				$f{ack}='phone'; $f{validity}='24'; $f{arch}=0; $f{flash}=0;
			};
			# ... x:data
			if (($f{username} eq '')&&($f{password} eq '')) {
				SendPluginPresencesToUser($PLUGIN_NAME,'unavailable',$from);
				delete $MiastoPlusa_accounts{$base_from};
			} else {
				for my $i (qw(username password ack validity arch flash)) { $account->{$i}=$f{$i}; };
				$MiastoPlusa_accounts{$base_from}=$account;
				SendPluginPresencesToUser($PLUGIN_NAME,'available',$from);
				PushAgentToUsersRoster($from); # important in any plugin which needs registration!
			};
#			SaveXMLHash(\%MiastoPlusa_accounts,'accounts',$config{plugins}->{$PLUGIN_NAME}->{accounts_file},$PLUGIN_NAME);
			$iq->SetType('result');
			$Connection->Send($iq);
		};
	};
};

sub MiastoPlusa_InMessage {
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

	my $account=$MiastoPlusa_accounts{$base_from};

	# the guts:

	my $result_message="";

	my $ua=new LWP::UserAgent;
	$ua->agent("Mozilla/3.0 (X11, I, Linux 2.4.0 i486"); # ;)
	$ua->timeout(30); # quickly please!
	$ua->no_proxy('www1.plus.pl');

	my $r=$ua->get('http://www1.plus.pl/sso/logowanie/login.html');
	if ($r->is_success) {
		$r=$ua->post('http://www1.plus.pl/sso/logowanie/auth',[
			op=>"login",
			login=>$account->{username},
			password=>$account->{password}]);
		if ($r->header("Location") !~ /invalid/) {
			my $sid=(grep(/^Set-Cookie: PD-H-SESSION-ID=/,split(/\n/,$r->headers->as_string)))[0];
			$sid=~s/^Set-Cookie: PD-H-SESSION-ID=//g; $sid=~s/;.+$//g;
			my $stick=(grep(/^Set-Cookie: AMWEBJCT!%2Fsso!stick=/,split(/\n/,$r->headers->as_string)))[0];
			$stick=~s/^Set-Cookie: AMWEBJCT!%2Fsso!stick=//g; $stick=~s/;.+$//g;
			my $ack=($account->{ack}?$account->{ack}:"none");
			my $ack_code;
			$ack_code=0 if ($ack ne 'web');
			$ack_code=10 if ($ack eq 'web');
			$ack_code=30 if ($ack eq 'phone');
			my %posthash=(
				smsType=>"10",
				phoneNumber=>$nr,
				userId=>"0",
				groupId=>"0",
				message=>convert("utf8","latin2",$message_o->GetBody()),
				preview=>":)",
				notifyCode=>$ack_code,
				validity=>($account->{validity}?$account->{validity}:'24'),
				sendDay=>'-1',sendHour=>'0',sendMin=>'0',
				templateCategory=>'0',
				targetURL=>'/sms/send_sms.jsp'
			);
			$posthash{archiveMessage}=1 if ($account->{arch} =~ /(true|yes|1)/);
			$posthash{flashMessage}=1 if ($account->{flash} =~ /(true|yes|1)/);
			$r=$ua->post("http://www1.plus.pl/rozrywka_i_informacje/sms/SendSMS2.do",\%posthash,Cookie=>"AMWEBJCT!%2Fsso!stick=$stick",Cookie=>"PD-H-SESSION-ID=$sid");
			{ # clean scope :P
				my @a=split(/\n/,$r->content); my $r=""; my $y=0;
				foreach my $l (@a) {
					$y=1 if ($l=~/-- content --/); # start matching here
					$y=0 if ($l=~/-- wyslij do --/); # and stop here
					$r.=$l."\n" if $y;
				};
				$r=~s/<[^>]+>//g; #tags
				$r=~s/Zasady/-/ig; # unneeded string
				$r=~s/\n//g; # newlines
				$r=~s/\&nbsp\;//g; # nbspaces
				$r=~s/^[0]* //g; # spaces at the beginning, and some 0 there too
				$r=~s/([0-9]+)/\ $1/g; # insert spaces in front of stats numbers
				$r=~s/do.*sieci//g; # superflous info
				$r=~s/Komunikat/\n/ig; #
				$r=~s/Do..?aduj/\n/ig;
				$r=~s/\s+/ /g; # anyspaces 
				$result_message=convert("latin2","utf8",$r);
			};
		} else { # login error? :D
			my $h=$r->header("Location");
			$h=~s/^.+errorMessage=//g; $h=~tr/+/ /; { no warnings; $h=~s/%(..)/pack('c', hex($1))/eg; };
			$result_message=convert("latin2","utf8",$h);
		};
	} else {
		$result_message="Connection to www gateway failed:(";
	};

	$message->SetBody("www.plus.pl: ".$result_message);
	$Connection->Send($message);
};

1;

