-- Apply once to a fresh Supabase project. No credentials belong in this file.
begin;
create schema if not exists cuebase_private;
revoke all on schema cuebase_private from public;
grant usage on schema cuebase_private to authenticated;

create table public.productions (
 id uuid primary key default gen_random_uuid(),
 owner_id uuid not null references auth.users(id),
 name text not null check (length(trim(name)) between 1 and 120),
 shared_settings jsonb not null default '{}',
 created_at timestamptz not null default now()
);
create table public.production_members (
 production_id uuid not null references public.productions(id) on delete cascade,
 user_id uuid not null references auth.users(id) on delete cascade,
 role text not null check (role in ('editor','viewer')),
 primary key (production_id,user_id)
);
create table public.channels (
 id uuid primary key default gen_random_uuid(),
 production_id uuid not null references public.productions(id) on delete cascade,
 name text not null check (length(trim(name)) between 1 and 120),
 description text not null default '',
 logo_path text,
 use_shared_settings boolean not null default false,
 settings jsonb not null default '{}',
 created_at timestamptz not null default now()
);
create index channels_production on public.channels(production_id);
create table public.channel_documents (
 channel_id uuid not null references public.channels(id) on delete cascade,
 document_id uuid not null,
 kind text not null check (kind in ('program','graphic','audio','rundown')),
 body jsonb not null,
 revision bigint not null default 1,
 updated_by uuid not null references auth.users(id),
 updated_at timestamptz not null default now(),
 primary key(channel_id,document_id)
);

create function cuebase_private.access_level(target uuid) returns text
language sql stable security definer set search_path = '' as $$
 select case when p.owner_id = auth.uid() then 'owner'
 else (select m.role from public.production_members m
       where m.production_id=p.id and m.user_id=auth.uid()) end
 from public.productions p where p.id=target
$$;
revoke all on function cuebase_private.access_level(uuid) from public;
grant execute on function cuebase_private.access_level(uuid) to authenticated;

alter table public.productions enable row level security;
alter table public.production_members enable row level security;
alter table public.channels enable row level security;
alter table public.channel_documents enable row level security;
create policy production_read on public.productions for select to authenticated
 using (cuebase_private.access_level(id) is not null);
create policy production_update on public.productions for update to authenticated
 using (owner_id=auth.uid()) with check (owner_id=auth.uid());
create policy member_read on public.production_members for select to authenticated
 using (cuebase_private.access_level(production_id) is not null);
create policy member_manage on public.production_members for all to authenticated
 using (cuebase_private.access_level(production_id)='owner')
 with check (cuebase_private.access_level(production_id)='owner');
create policy channel_read on public.channels for select to authenticated
 using (cuebase_private.access_level(production_id) is not null);
create policy channel_manage on public.channels for all to authenticated
 using (cuebase_private.access_level(production_id)='owner')
 with check (cuebase_private.access_level(production_id)='owner');
create policy document_read on public.channel_documents for select to authenticated
 using (exists(select 1 from public.channels c where c.id=channel_id
 and cuebase_private.access_level(c.production_id) is not null));

-- Production and its one default channel are created in one transaction.
create function public.create_production(production_name text,channel_name text)
 returns jsonb language plpgsql security definer set search_path = '' as $$
declare p uuid; c uuid;
begin
 if auth.uid() is null then raise exception 'Sign in first'; end if;
 insert into public.productions(owner_id,name) values(auth.uid(),production_name)
 returning id into p;
 insert into public.channels(production_id,name) values(p,channel_name) returning id into c;
 return jsonb_build_object('production_id',p,'channel_id',c);
end $$;

-- Revision zero means create. A stale revision never overwrites another editor.
create function public.save_channel_document(target_channel uuid,target_document uuid,
 document_kind text,expected_revision bigint,content jsonb)
 returns bigint language plpgsql security definer set search_path = '' as $$
declare production uuid; current_revision bigint; next_revision bigint;
begin
 select production_id into production from public.channels where id=target_channel;
 if coalesce(cuebase_private.access_level(production),'') not in ('owner','editor')
 then raise exception 'No edit permission' using errcode='42501'; end if;
 if expected_revision=0 then
   insert into public.channel_documents(channel_id,document_id,kind,body,updated_by)
   values(target_channel,target_document,document_kind,content,auth.uid());
   return 1;
 end if;
 select revision into current_revision from public.channel_documents
 where channel_id=target_channel and document_id=target_document for update;
 if current_revision is null or current_revision<>expected_revision then
   raise exception 'Document changed; reload before saving' using errcode='40001';
 end if;
 next_revision=current_revision+1;
 update public.channel_documents set kind=document_kind,body=content,
 revision=next_revision,updated_by=auth.uid(),updated_at=now()
 where channel_id=target_channel and document_id=target_document;
 return next_revision;
end $$;
revoke all on function public.create_production(text,text) from public;
revoke all on function public.save_channel_document(uuid,uuid,text,bigint,jsonb) from public;
grant execute on function public.create_production(text,text) to authenticated;
grant execute on function public.save_channel_document(uuid,uuid,text,bigint,jsonb) to authenticated;
revoke all on public.productions,public.production_members,public.channels,public.channel_documents from anon,authenticated;
grant select on public.productions,public.production_members,public.channels,public.channel_documents to authenticated;
grant update(name,shared_settings) on public.productions to authenticated;
grant insert,update,delete on public.production_members,public.channels to authenticated;
commit;
